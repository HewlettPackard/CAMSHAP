# CAMSHAP: Accelerating Machine Learning Model Explainability with Analog CAM

## Overview
This repository contains the simulation code used in the paper "CAMSHAP: Accelerating Machine Learning Model Explainability with Analog CAM", accepted for presentation at 2024 ACM/IEEE International Conference on Computer-Aided Design (ICCAD 2024). The code is used to evaluate the performance of the CAMSHAP architecture for accelerating machine learning model explainability. 

## Table of Contents
1. [Introduction](#Introduction)
2. [Building and Installation](#Building-and-Installation)
    1. [Dependencies](#Dependencies)
    2. [Build](#Build)
    3. [Install](#Install)
    4. [Verify](#Verify)
3. [Usage](#Usage)
4. [Example](#Example)


## Introduction
Content addressable memory (CAM) searches its memory block to see if input data is stored in it and returns the address where the data is found. Analog CAM stores a range of analog values in its memory cells and performs a parallel search to find whether input data is within the range or not.

Recently, we proposed X-TIME[^1], an in-memory engine that uses analog CAMs to accelerate inference of tree-based machine learning model. Thanks to the parallel search capability of analog CAMs, it achieves 119× lower latency at 9740× higher throughput compared to GPU for inference. 

CAMSHAP[^2] is an extended version of X-TIME that accelerates model explainability for tree-based machine learning models. To support both inference and model explanation, an analog CAM-based architecture with custom instructions is developed to integrate the CAM and other functional components with the processor core. The CAMSHAP architecture is evaluated on five different datasets, showing 10× higher throughput, 191× smaller latency, and 41× smaller energy consumption compared to GPU for explaining tree-based machine learning model outputs.

The CAMSHAP architecture is implemented in Structural Simulation Toolkit (SST) to evaluate the performance of the architecture. SST provides standard components, such as processor and memory, and allows to perform a discrete-event simulation in parallel with MPI. Customized components such as analog CAM, MPE, and MMR are implemented as loadable modules in SST.

## Building and Installation

### Dependencies
To use SST, you need to install SST-Core on your workspace following the installation guide.

[SST Installation guide](https://sst-simulator.org/SSTPages/SSTBuildAndInstall_14dot0dot0_SeriesDetailedBuildInstructions/)

Once you install the SST-Core, verify the SST-Core installation is correct.
```sh
export SST_CORE_HOME=[path to sst-core build location ('--prefix' in sst-core build)]
export PATH=$SST_CORE_HOME/bin:$PATH
which sst
sst --version
sst-info
sst-test-core
```

### Build
To get started, clone this repository and run `autogen.sh` and `run_cofnig.sh` for configuration.
```sh
git clone https://github.com/HewlettPackard/CAMSHAP.git
cd camshap
./autogen.sh
./run_config.sh
make 
```

### Install
Installation is really registering your module into SST's module database. Once you install your module, it informs SST where it is located on the system and then you only have to reference the main SST executable. Whenever you make change to the source/header files, it should be installed again.
```
make install
```

### Verify
You can verify the module built, registered, and is loadable by SST by running `sst-info`, a small program to parse and dump all registerd module information known to SST.
```sh
# Dump all module info
sst-info

# Dump and verify camshap
sst-info camshap

```
[sst-info doc](http://sst-simulator.org/SSTPages/SSTToolSSTInfo/)

## Usage
You need to program a Python script to create SST components and connect them together. The SST will execute the Python config file and run the simulation. 
The format of SST command is:
```sh
sst [SST options] <test file>
```
`[SST options]` is the simulator options, which is optional. For example, the number of parallel threads per rank (-n). You can find the details at the website (http://sst-simulator.org/SSTPages/SSTUserHowToRunSST/) or by typing `sst -h`
`<test file>` is the Python file that creates SST compoenents, assigns their parameters, defines the links connecting them, and interconnect them. The parameters for Python config file may follow it.

The example command of this repository is:
```sh
sst ./tests/test_camshap.py -- --dataset=churn --model=xgboost_tree800_depth10_trial0 --folder=churn_f100b100_v10_p0
```
`./src/tests/test_camshap.py` is a Python config file building the CAMSHAP architecture. `-- --dataset=churn --model=xgboost_tree800_depth10_trial0 --folder=churn_f100b100_v10_p0` is the paramters for Python config file, which means the dataset is 'churn', the name of tree-based ML model in `./model/churn/` folder is 'xgboost_tree800_depth10_trial0', and the name of simulation result folder in `log` folder is 'churn_f100b100_v10_p0'.

To see the parameters of Python config file, you can do:
```sh
sst ./tests/test_camshap.py -- -h

usage: sstsim.x [-h] [--dataset DATASET] [--model MODEL] [--folder FOLDER]

optional arguments:
  -h, --help         show this help message and exit
  --dataset DATASET  Name of dataset
  --model MODEL      Name of tree-based ML model in ./model/DATASET/ folder
  --folder FOLDER    Name of simulation result folder in ./log/ folder

```

The parameters for simulation setup and hardware components are defined in the configuration file (`./json/sw.json` and `./json/hw.json`). You can modify the parameters in the configuration file to change the simulation setup and hardware components.

### Directory Structure

```sh
camshap/
├── json/                   # Directory for configuration files
│   ├── sw.json             # Simulation setup configuration
│   └── hw.json             # Hardware components configuration
├── log/                    # Directory for output results
├── model/                  # Directory for input models and datasets
├── plots/                  # Directory for ploting figures
├── src/                    # Source code directory
├── tests/                  # Test code directory
│   ├── configure.py        # Script to configure the SST components
│   ├── noc.py              # Script to build the modules in SST and connect them
│   ├── util.py             # Script to convert the model/dataset to the format that the modules in SST can read
│   └── test_camshap.py     # Main script to run the simulations
└── README.md               # This file
```

## Example
```sh
mpirun -n 20 sst ./tests/test_camshap.py -- --dataset=gesture --model=xgboost_tree300_depth10_trial4 --folder=gesture_f100b100_v10000_p0
mpirun -n 20 sst ./tests/test_camshap.py -- --dataset=mortality --model=xgboost_tree900_depth9_trial2 --folder=mortality_f100b100_v10_p0
mpirun -n 20 sst ./tests/test_camshap.py -- --dataset=telco --model=xgboost_tree900_depth10_trial2 --folder=telco_f100b100_v100_p0
mpirun -n 20 sst ./tests/test_camshap.py -- --dataset=churn --model=xgboost_tree800_depth10_trial0 --folder=churn_f100b100_v10_p0
mpirun -n 20 sst ./tests/test_camshap.py -- --dataset=eye --model=xgboost_tree500_depth8_trial3 --folder=eye_f100b100_v10000_p0
```

## License
CAMSHAP is licensed under [MIT](https://github.com/HewlettPackard/CAMSHAP/blob/master/LICENSE) license.

[^1]: Pedretti, G., Moon, J., Bruel, P. et al. X-TIME: An in-memory engine for accelerating machine learning on tabular data with CAMs. arXiv:2304.01285 (2023). https://arxiv.org/abs/2304.01285
[^2]: Moon, J., Pedretti, G., Bruel, P. et al. CAMSHAP: Accelerating Machine Learning Model Explainability with Analog CAM. Accepted at ICCAD 2024.