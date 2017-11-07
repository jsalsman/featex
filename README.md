# featex
PocketSphinx phonetic feature extraction for intelligibility prediction and remediation

To compile and run, you need to install CMU PocketSphinx, e.g., on redhat/centos/fedora:

    sudo yum install svn autoconf libtool automake bison python-devel swig

or, on debian/ubuntu/mint:

    sudo apt-get install svn autoconf libtool automake bison python-dev swig

then:

    cd 
    mkdir src
    cd src
    mkdir ps
    cd ps
    svn checkout svn://svn.code.sf.net/p/cmusphinx/code/trunk/sphinxbase
    cd sphinxbase
    ./autogen.sh
    make
    sudo make install
    cd ..
    svn checkout svn://svn.code.sf.net/p/cmusphinx/code/trunk/pocketsphinx
    cd pocketsphinx
    ./autogen.sh
    make
    sudo make install
    
    cd test/regression
    ./test-lm.sh

This should say, "All sub-tests passed" 

    cd ~/src/ps
    git clone https://github.com/jsalsman/featex
    cd featex
    
    gcc -I /usr/local/include/pocketsphinx -I /usr/local/include/sphinxbase \
        -I ~/src/ps/pocketsphinx/src/libpocketsphinx \
        -o featex featex.c -lpocketsphinx -lsphinxbase -lm
    
    LD_LIBRARY_PATH=/usr/local/lib ./featex we drank tea in the afternoon and watched tv

Make sure those `-I` include paths and the `#define MODELDIR` directive near the top of featex.c correctly identify where the include files, libraries, and en-us model files were actually installed. The numeric feature stream goes to standard output, and verbose debugging output goes to stderr, so in production you might likely run it with `2>/dev/null`. If you don't want to prepend the LD_LIBRARY_PATH, which is necessary on redhat but not debian variants, see e.g. https://serverfault.com/a/372998

The file `Spoken-English-Intelligibility-Remediation.pdf` -- also at http://arxiv.org/abs/1709.01713 -- has more information.
