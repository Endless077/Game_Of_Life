#!/bin/bash

#===============================================================================
# Full GCP MPI Cluster Provisioner (clone)
#
# 1. Provision one master and three worker VMs.
# 2. Install OpenMPI 4.1.8 on the master, create a golden image.
# 3. Configure password-less SSH for the “cluster” user across all nodes.
# 4. Distribute, build and launch the MPI application.

# with full logging.
#===============================================================================

set -euo pipefail

#--------------------------------------
# User-configurable variables
#--------------------------------------
PROJECT_ID=""                               # ← GCP project ID
ZONE=""                                     # ← preferred zone
MACHINE_TYPE=""                             # ← VM type
IMAGE_FAMILY=""                             # ← image family
IMAGE_SIZE="10GB"                           # ← image size
IMAGE_PROJECT=""                            # ← image project
MASTER_NAME="hpc-master"                    # ← master name
IMAGE_NAME="openmpi-golden"                 # ← golden image name
SLAVE_PREFIX="hpc-node"                     # ← slave instances prefix
SLAVE_COUNT=3                               # ← number of slaves
NODE_SLOTS=8                                # ← slots per node (vCPUs)

# MPI job parameters (from script args):
GRID_SIZE=${1:?Usage: $0 GRID_SIZE ITERATIONS [LOCAL_CODE_DIR]}
ITERATIONS=${2:?Usage: $0 GRID_SIZE ITERATIONS [LOCAL_CODE_DIR]}

# Path to your local project directory
CODE_DIR_LOCAL=${3:-"./HPC_Project/source/project"}

TOTAL_PROCS=$(( (SLAVE_COUNT + 1) * NODE_SLOTS ))

#--------------------------------------
# Prepare GCP project
#--------------------------------------
echo "Setting GCP project to ${PROJECT_ID}"
gcloud config set project "${PROJECT_ID}"

#--------------------------------------
# Create master startup script
#--------------------------------------
cat << 'EOF' > startup-master.sh
#!/bin/bash
# Logging all output to log file and to serial console
echo "-- Logging Startup Script (master) --" | tee /var/log/startup.log
exec > >(tee -a /var/log/startup.log) 2>&1

# Update packages and install dependencies
echo "Updating and Upgrading package lists..."
apt-get update -y
apt-get upgrade -y

# Install basic tools
echo "Installing dependencies..."
apt-get install -y vim htop build-essential openssh-server openssh-client wget bzip2 make cmake curl

# Download and install OpenMPI 4.1.8
echo "Downloading and installing OpenMPI 4.1.8..."
cd /tmp
wget https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-4.1.8.tar.bz2 -O openmpi-4.1.8.tar.bz2
tar -xjf openmpi-4.1.8.tar.bz2
cd openmpi-4.1.8
./configure --prefix=/usr/local
make -j"$(nproc)"
make install
ldconfig

# Creating user 'cluster' with sudo privileges
echo "Creating user 'cluster' with sudo privileges..."
useradd -m -s /bin/bash -G sudo cluster
echo "cluster:root" | chpasswd

echo "Preparing SSH dir for 'cluster'..."
mkdir -p /home/cluster/.ssh
chown cluster:cluster /home/cluster/.ssh
chmod 700 /home/cluster/.ssh

# Adding LD_LIBRARY_PATH
echo "Adding LD_LIBRARY_PATH"
echo "export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH" >> /home/cluster/.bashrc

echo "OpenMPI version:" >> /var/log/startup.log
mpirun --version   >> /var/log/startup.log 2>&1

echo "-- End of Startup Script --" | tee -a /var/log/startup.log
EOF

#--------------------------------------
# Spin up master VM
#--------------------------------------
echo "Creating master VM: ${MASTER_NAME}"
gcloud compute instances create "${MASTER_NAME}" \
    --project="${PROJECT_ID}" \
    --zone="${ZONE}" \
    --description="HPC Master for MPI cluster" \
    --machine-type="${MACHINE_TYPE}" \
    --network-interface=network-tier=PREMIUM,stack-type=IPV4_ONLY,subnet=default \
    --metadata-from-file startup-script=startup-master.sh \
    --metadata=ssh-keys=cluster:$(cat ~/.ssh/id_rsa.pub) \
    --no-restart-on-failure \
    --maintenance-policy=TERMINATE \
    --provisioning-model=SPOT \
    --instance-termination-action=STOP \
    --service-account="${PROJECT_ID}@developer.gserviceaccount.com" \
    --scopes=https://www.googleapis.com/auth/devstorage.read_only,https://www.googleapis.com/auth/logging.write,https://www.googleapis.com/auth/monitoring.write \
    --tags=http-server,https-server,lb-health-check \
    --create-disk=auto-delete=yes,boot=yes,device-name="${MASTER_NAME}",image=projects/ubuntu-os-cloud/global/images/ubuntu-2004-focal-v20250213,mode=rw,size="${IMAGE_SIZE}",type=pd-balanced \
    --no-shielded-secure-boot \
    --shielded-vtpm \
    --shielded-integrity-monitoring \
    --labels=role=mpi-master \
    --reservation-affinity=any

#--------------------------------------
# Wait and display installation log of master
#--------------------------------------
echo "Waiting for master installation to finish (60s)..."
sleep 60

echo "Master installation log:"
gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="sudo cat /var/log/startup.log"

#--------------------------------------
# Create golden image from master
#--------------------------------------
echo "Creating golden image '${IMAGE_NAME}' from master disk"
gcloud compute images create "${IMAGE_NAME}" \
  --source-disk="${MASTER_NAME}" \
  --source-disk-zone="${ZONE}" \
  --storage-location="${ZONE}"

#--------------------------------------
# Create slave startup script
#--------------------------------------
cat << 'EOF' > startup-slave.sh
#!/bin/bash
# Regenerate SSH host keys
echo "Regenerating SSH host keys..."
rm -f /etc/ssh/ssh_host_*
dpkg-reconfigure openssh-server

# Reset hostname from metadata
echo "Setting hostname from metadata..."
HOSTNAME=$(curl -H "Metadata-Flavor: Google" \
  http://metadata.google.internal/computeMetadata/v1/instance/name)
hostnamectl set-hostname "${HOSTNAME}"
EOF

#--------------------------------------
# Spin up slave VMs from golden image
#--------------------------------------
echo "Creating ${SLAVE_COUNT} slave VMs from image '${IMAGE_NAME}'"
for i in $(seq 1 "${SLAVE_COUNT}"); do
  NAME="${SLAVE_PREFIX}-${i}"
  echo ">> Creating slave VM: ${NAME}"
  gcloud compute instances create "${NAME}" \
      --project="${PROJECT_ID}" \
      --zone="${ZONE}" \
      --description="HPC Slave for MPI cluster" \
      --machine-type="${MACHINE_TYPE}" \
      --network-interface=network-tier=PREMIUM,stack-type=IPV4_ONLY,subnet=default \
      --metadata-from-file startup-slave.sh \
      --metadata=ssh-keys=cluster:$(cat ~/.ssh/id_rsa.pub) \
      --no-restart-on-failure \
      --maintenance-policy=TERMINATE \
      --provisioning-model=SPOT \
      --instance-termination-action=STOP \
      --service-account="${PROJECT_ID}@developer.gserviceaccount.com" \
      --scopes=https://www.googleapis.com/auth/devstorage.read_only,https://www.googleapis.com/auth/logging.write,https://www.googleapis.com/auth/monitoring.write \
      --tags=http-server,https-server,lb-health-check \
      --create-disk=auto-delete=yes,boot=yes,device-name="${NAME}",image=projects/ubuntu-os-cloud/global/images/ubuntu-2004-focal-v20250213,mode=rw,size="${IMAGE_SIZE}",type=pd-balanced \
      --no-shielded-secure-boot \
      --shielded-vtpm \
      --shielded-integrity-monitoring \
      --labels=role=mpi-worker \
      --reservation-affinity=any
  echo "Slave started: ${NAME}"
done

#--------------------------------------
# Wait before further configuration
#--------------------------------------
echo "Waiting for slaves to initialize (60s)..."
sleep 60

#--------------------------------------
# Configure password-less SSH for user 'cluster'
#--------------------------------------
echo "Generating SSH key on master for 'cluster'"
gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="
  sudo -u cluster ssh-keygen -t rsa -b 4096 -f /home/cluster/.ssh/id_rsa -q -N ''
"

PUBKEY=$(gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="sudo cat /home/cluster/.ssh/id_rsa.pub")

echo "Distributing public key to all nodes for 'cluster'"
for HOST in "${MASTER_NAME}" $(for i in $(seq 1 "${SLAVE_COUNT}"); do echo "${SLAVE_PREFIX}-${i}"; done); do
  gcloud compute ssh "${HOST}" --zone="${ZONE}" --command="
    sudo mkdir -p /home/cluster/.ssh &&
    echo '${PUBKEY}' | sudo tee -a /home/cluster/.ssh/authorized_keys &&
    sudo chown -R cluster:cluster /home/cluster/.ssh &&
    sudo chmod 700 /home/cluster/.ssh &&
    sudo chmod 600 /home/cluster/.ssh/authorized_keys
  "
done

#--------------------------------------
# Create MPI hostfile on master
#--------------------------------------
echo "Creating mpi_hosts.txt"
gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="
  cat <<EOL > /home/cluster/mpi_hosts.txt
${MASTER_NAME} slots=${NODE_SLOTS}
$(for i in $(seq 1 "${SLAVE_COUNT}"); do echo "${SLAVE_PREFIX}-${i} slots=${NODE_SLOTS}"; done)
EOL && sudo chown cluster:cluster /home/cluster/mpi_hosts.txt
"

#--------------------------------------
# Copy project to master & compile
#--------------------------------------
echo "Preparing project directory on master"
gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="
  sudo mkdir -p /home/cluster/hpc-code && sudo chown cluster:cluster /home/cluster/hpc-code
"

echo "Copying project source to master"
gcloud compute scp --recurse "${CODE_DIR_LOCAL}/." cluster@"${MASTER_NAME}":/home/cluster/hpc-code --zone="${ZONE}"

echo "Building project with make build"
gcloud compute ssh cluster@"${MASTER_NAME}" --zone="${ZONE}" --command="
  cd /home/cluster/hpc-code && make build
"

#--------------------------------------
# Distribute executable to slaves
#--------------------------------------
echo "Distributing binary to slaves"
for i in $(seq 1 "${SLAVE_COUNT}"); do
  SLAVE="${SLAVE_PREFIX}-${i}"
  gcloud compute scp cluster@"${MASTER_NAME}":/home/cluster/hpc-code/game_of_life cluster@"${SLAVE}":/home/cluster/hpc-code --zone="${ZONE}"
done

#--------------------------------------
# Launch MPI job
#--------------------------------------
echo "Launching MPI job"
gcloud compute ssh cluster@"${MASTER_NAME}" --zone="${ZONE}" --command="
  cd /home/cluster/hpc-code &&
  mpirun --hostfile /home/cluster/mpi_hosts.txt -np ${TOTAL_PROCS} ./game_of_life ${GRID_SIZE} ${ITERATIONS}
"

echo "All done!"
