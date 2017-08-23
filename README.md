# featex
PocketSphinx phonetic feature extraction for intelligibility prediction and remediation

To compile and run, you need to install CMU PocketSphinx, e.g.:

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
    
    cd ~/src/ps
    git clone https://github.com/jsalsman/featex
    cd featex
    
    gcc -I /usr/local/include/pocketsphinx -I /usr/local/include/sphinxbase \
        -I ~/src/ps/pocketsphinx/src/libpocketsphinx \
        -o featex featex.c -lpocketsphinx -lsphinxbase -lm
    
    ./featex we drank tea in the afternoon and watched tv

Make sure those `-I` include paths and the `#define MODELDIR` directive near the top of featex.c correctly identify where the include files, libraries, and en-us model files were actually installed. The numeric feature stream goes to standard output, and verbose debugging output goes to stderr, so in production you might likely run3 it with `2>/dev/null`.
