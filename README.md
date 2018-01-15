This repository hosts the sources of Polly for FPGA based on Polly 5.0.0.  
How to build Polly for SPGen  

1. download LLVM from:  
http://releases.llvm.org/5.0.0/llvm-5.0.0.src.tar.xz  
2. make a workspace, for example:  
mkdir $HOME/LLVM  
3. extract the xz file at $HOME/LLVM  
clone this repository at $HOME/LLVM/llvm-5.0.0-src/tools  
4. make a build directory for LLVM:  
mkdir $(HOME)/LLVM/build  
5. prepare build at $(HOME)/LLVM/build  
cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX={LLVM_INSTALL_PATH_YOU_WANT} ../llvm-5.0.0.src/  
6. make and make install  
7. set PATH, LD_LIBRARY_PATH for LLVM
