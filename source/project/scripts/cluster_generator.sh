#!/bin/bash

#===============================================================================
# Full GCP MPI Cluster Provisioner (generation)
#
# 1. Provision one master and three worker VMs.
# 2. Install OpenMPI 4.1.8 on the master.
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
IMAGE_PROJECT=""                            # ← image project
IMAGE_SIZE="10GB"                           # ← image size
MASTER_NAME="hpc-master"                    # ← master instance name
SLAVE_PREFIX="hpc-node"                     # ← slave instance prefix
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
echo ">> Setting GCP project to ${PROJECT_ID}"
gcloud config set project "${PROJECT_ID}"

#--------------------------------------
# Create startup script for VM initialization
#--------------------------------------
cat << 'EOF' > startup.sh
#!/bin/bash
# Logging all output to log file and to serial console
echo "-- Logging Startup Script --" | tee /var/log/startup.log
exec > >(tee -a /var/log/startup.log) 2>&1

# Update packages and install dependencies
echo "Updating and Upgrading package lists..."
apt-get update -y
apt-get upgrade -y

# Install basic tools
echo "Installing dependencies..."
apt-get -y install vim htop build-essential openssh-server openssh-client wget bzip2 make cmake

# Download and install OpenMPI 4.1.8
echo "Downloading and installing OpenMPI 4.1.8..."
cd /tmp
wget https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-4.1.8.tar.bz2 -O openmpi-4.1.8.tar.bz2
tar -xjf openmpi-4.1.8.tar.bz2
cd openmpi-4.1.8
./configure --prefix=/usr/local
make -j$(nproc)
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
echo ">> Creating master VM: ${MASTER_NAME}"
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
# Spin up slave VMs
#--------------------------------------
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
done

#--------------------------------------
# Wait for all VMs to finish startup
#--------------------------------------
echo ">> Waiting 60s for VMs to initialize…"
sleep 60

#--------------------------------------
# Generate SSH keypair on master for user 'cluster'
#--------------------------------------
echo ">> Generating RSA keypair on master as user 'cluster'"
gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="
  sudo -u cluster ssh-keygen -t rsa -b 4096 -f /home/cluster/.ssh/id_rsa -q -N ''
"

# Retrieve the public key
PUBKEY=$( gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="sudo cat /home/cluster/.ssh/id_rsa.pub" )

#--------------------------------------
# Distribute public key into cluster’s authorized_keys on every node
#--------------------------------------
echo ">> Distributing public key to 'cluster' on all nodes"
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
# Build MPI hostfile on master
#--------------------------------------
echo ">> Creating mpi_hosts.txt on master"
gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="
  sudo bash -c 'cat <<EOL > /home/cluster/mpi_hosts.txt
${MASTER_NAME} slots=${NODE_SLOTS}
$(for i in \$(seq 1 \"${SLAVE_COUNT}\"); do echo \"${SLAVE_PREFIX}-\$i slots=${NODE_SLOTS}\"; done)
EOL'
  sudo chown cluster:cluster /home/cluster/mpi_hosts.txt
"

#--------------------------------------
# Copy project to master & compile with make build
#--------------------------------------
echo ">> Copying project to master and running 'make build' as 'cluster'"
gcloud compute ssh "${MASTER_NAME}" --zone="${ZONE}" --command="
  sudo mkdir -p /home/cluster/hpc-code && sudo chown cluster:cluster /home/cluster/hpc-code
"
gcloud compute scp --recurse "${CODE_DIR_LOCAL}" cluster@"${MASTER_NAME}":/home/cluster/hpc-code --zone="${ZONE}"
gcloud compute ssh cluster@"${MASTER_NAME}" --zone="${ZONE}" --command="
  cd ~/hpc-code &&
  make build
"

#--------------------------------------
# Distribute the executable to all slaves
#--------------------------------------
echo ">> Distributing built executable to slaves"
for i in $(seq 1 "${SLAVE_COUNT}"); do
  SLAVE="${SLAVE_PREFIX}-${i}"
  gcloud compute scp cluster@"${MASTER_NAME}":/home/cluster/hpc-code/game_of_life cluster@"${SLAVE}":/home/cluster/hpc-code --zone="${ZONE}"
done

#--------------------------------------
# Launch the MPI job from the master
#--------------------------------------
echo ">> Running MPI job on the cluster as 'cluster'"
gcloud compute ssh cluster@"${MASTER_NAME}" --zone="${ZONE}" --command="
  cd ~/hpc-code &&
  mpirun --hostfile ~/mpi_hosts.txt -np ${TOTAL_PROCS} ./game_of_life ${GRID_SIZE} ${ITERATIONS}
"

echo ">> All done!"
