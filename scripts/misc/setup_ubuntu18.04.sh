#!/bin/sh
# @file scripts/misc/setup_ubuntu18.04.sh
#
# @author Hiroyuki Chishiro
#

prefix="sudo apt-get -y install"

# compiler
$prefix ccache gcc clang clang-tools llvm gcc-aarch64-linux-gnu

# C analyze tool
$prefix astyle cppcheck flawfinder

# python
$prefix python3 python3-pip

# python analyze tool
pip3 install pyflakes pylint 

# tool
$prefix git samba make doxygen sendmail git

# LaTeX
#$prefix tetex-* texlive-latex* texlive-science ptex-jtex
#$prefix texlive-lang-japanese texlive-fonts-recommended
#$prefix dvi2ps-fontdesc-morisawa5

# QEMU
$prefix qemu qemu-system libncurses5-dev libncursesw5-dev


echo "\n"
echo "If you use QEMU 2.12.0 (or later) for Raspberry Pi3 (AARCH64), please use https://www.qemu.org/download/."
echo "If you use AXIS ISA for DOM, please use https://github.com/pflab-ut/llvm."
