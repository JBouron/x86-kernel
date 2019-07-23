# This docker image contains all the tool chain required to build the kernel.
FROM ubuntu:latest
RUN apt-get update -y -o Acquire::ForceIPv4=true && apt-get upgrade -y -o Acquire::ForceIPv4=true
#RUN apt-get install -y -o Acquire::ForceIPv4=true build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libcloog-isl-dev libisl-dev curl
RUN apt-get install -y -o Acquire::ForceIPv4=true build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-dev curl
RUN mkdir /home/src
RUN curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.32.tar.xz
RUN curl -LO https://ftp.gnu.org/gnu/gcc/gcc-9.1.0/gcc-9.1.0.tar.xz
RUN tar xvf binutils-2.32.tar.xz
RUN tar xvf gcc-9.1.0.tar.xz
COPY mkbinutils.sh /
RUN chmod +x /mkbinutils.sh && /mkbinutils.sh
COPY mkgcc.sh /
RUN chmod +x /mkgcc.sh && /mkgcc.sh
