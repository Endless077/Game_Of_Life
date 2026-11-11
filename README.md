![Wallpaper](https://github.com/Endless077/Game_Of_Life_OpenMPI/blob/main/game_of_life.jpeg)

# Game of Life 🧬

This project is a parallel simulation of **Conway’s Game of Life** written in **C** using **OpenMPI**. It demonstrates distributed memory computing through message passing and ghost cell synchronization across multiple processes.


## 🔑 Key Features

- **📐 Flexible Board Dimensions**: Supports rectangular boards (`N × M`) with any number of rows/columns.
- **⚙️ Parallel Execution with MPI**: The board is partitioned across `P` processes using **row-wise scattering**.
- **👻 Ghost Rows**: Each process maintains top/bottom ghost rows for correct neighbor computation.
- **🧪 Early Exit Conditions**:
  - All cells are dead (zero population)
  - Board reaches a **bitwise steady state**
  - **Alive-cell count** unchanged for a number of generations
- **📈 Performance Monitoring**: Timings and statistics are printed at each generation from the master process.


## 🙏 Acknowledgements

- John Conway – Inventor of the original Game of Life.
- OpenMPI – High-performance message passing framework.
- CMake – Cross-platform build automation tool.
- GNU Make – Build control and automation.


## 🧠 Simulation Logic

The board is split **row-wise** between processes. Each rank performs the simulation locally on its chunk of the board.

- **Initialization**
  - Rank 0 creates and initializes the full board with random values (ensuring at least 1 alive cell).
  - The board is scattered among processes using `MPI_Scatterv`.
  - Each rank receives a padded buffer with ghost rows.

- **Main Simulation Loop**
  - Each rank:
    - Exchanges ghost rows with neighbors via `MPI_Sendrecv`.
    - Computes the next generation using `life_step`.
    - Performs early-exit checks:
      - **Steady state**: no changes from the previous generation.
      - **Zero population**: all cells are dead.
      - **Stable population**: alive-cell count unchanged for 10 consecutive generations.
    - Buffers are swapped for the next iteration.
    - Global alive-cell count is reduced with `MPI_Reduce`.
    - Master prints statistics per generation (elapsed time, alive cells).

- **Finalization**
  - All ranks free their local memory.
  - MPI is finalized with `MPI_Finalize()`.
  - Rank 0 prints the final summary and total simulation time.


## 📦 Installation

### 🧰 Requirements

- C Compiler (e.g., `gcc`, `clang`)
- [OpenMPI](https://www.open-mpi.org/)
- CMake
- Make

### 🔧 Build Instructions

```bash
git clone https://github.com/your-username/game-of-life-mpi.git
cd game-of-life-mpi
make
```

## 🚀 Run the Simulation

Use make run with the required parameters:

```bash
make run N=<rows> M=<cols> E=<epoch> P=<nprocs> [S=<seed>]
mpirun -np P ./build/game_fo_life -n N -m M -e E [-s S]
```


## 📚 Additional MPI Exercises

This repository includes other hands-on MPI exercises that demonstrate core concepts in parallel programming using the **Message Passing Interface (MPI)**. These are provided as standalone programs or examples that can be compiled and run separately from the Game of Life simulation.

### 📦 Included Exercises

- **MPI Point-to-Point Communication**
  - Demonstrates basic `MPI_Send` and `MPI_Recv` mechanics between ranks.
  - Useful for understanding how explicit messaging works in distributed systems.

- **MPI Non-blocking Communication**
  - Explores `MPI_Isend` and `MPI_Irecv` along with `MPI_Wait` / `MPI_Test`.
  - Useful for overlapping computation and communication.

- **MPI Collective Communication**
  - Shows examples of collective operations such as:
    - `MPI_Bcast`
    - `MPI_Scatterv`
    - `MPI_Gather`
    - `MPI_Reduce` / `MPI_Allreduce`
  - Demonstrates how data can be efficiently shared among all ranks.


## 🧪 Utility Scripts

The repository also includes a few **Bash scripts** to assist with development, deployment, and testing:

- `cluster_generator.sh`: 
  - Automates the generation of hostfiles or cluster config for MPI runs.
  
- `cluster_cloner.sh`: 
  - Used to clone or sync your codebase across nodes in a cluster setup (e.g., via SSH or SCP).
  
- `scalability_test_script.sh`: 
  - Script to benchmark the scalability of the Game of Life simulation across different numbers of processes and board sizes.

These scripts are intended to **streamline testing and deployment in real or virtual HPC environments**.


## 💾 License

This project is licensed under the GNU General Public License v3.0.

[GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.en.html)

![Static Badge](https://img.shields.io/badge/UniSA-GameOfLife-red?style=plastic)


## 🖐 Authors

**Project Manager:**
- [Antonio Garofalo](https://github.com/Endless077)


## 🔔 Support

For support, feel free to reach out via [GitHub](https://github.com/Endless077) or contact the project contributors.


## 📝 Documentation

No documentation, see this website [here](https://spagnuolocarmine.github.io).
