# featex
PocketSphinx phonetic feature extraction for intelligibility prediction and remediation

### Dependencies

#### Packages

To compile and run, you need to install CMU PocketSphinx
On Redhat/CentOS/Fedora:

    sudo yum install svn autoconf libtool automake bison python-devel swig gcc make

or on Debian based systems:

    sudo apt-get install subversion autoconf libtool automake bison python-dev swig gcc make

#### Sphinxbase and Pocketsphinx

Install both sphinxbase and pocketsphinx. Go to any temp directory and execute the following.

Sphinxbase:

    $ svn checkout svn://svn.code.sf.net/p/cmusphinx/code/trunk/sphinxbase
    $ cd sphinxbase
    $ ./autogen.sh
    $ make
    $ sudo make install

Pocketsphinx:

    $ svn checkout svn://svn.code.sf.net/p/cmusphinx/code/trunk/ pocketsphinx
    $ cd pocketsphinx
    $ ./autogen.sh
    $ make
    $ sudo make install

Run the builtin test:

    $ cd test/regression
    $ ./test-lm.sh

This should say, "All sub-tests passed"

### Compiling

Clone the repository:

    $ git clone https://github.com/jarulsamy/featex

Create a build directory and `cd` into it:

    $ cd featex
    $ mkdir build
    $ cd build

Generate the build files with `cmake`:

    $ cmake ..

Finally, compile:

    $ make

>The output binary, `featex.o` should be in the `bin/` directory.


### Usage

View the built-in help with:

    $ ./featex --help

    Usage: featex.o [OPTION...]
    featex -- PocketSphinx phonetic feature extraction for intelligibility
    prediction and remediation

    -c, --combo=COMBO_PATH     Path to combo.dict.
    -d, --diphones             Toggle play diphones
    -i, --infile=INFILE_PATH   Path to input raw file.
    -p, --phonemes             Toggle play phonemes
    -P, --phrase='PHRASE'      Input phrase
    -t, --triphones            Toggle play triphones
    -u, --utterance            Toggle play utterances
    -w, --word                 Toggle play words
    -?, --help                 Give this help list
        --usage                Give a short usage message
    -V, --version              Print program version

    Mandatory or optional arguments to long options are also mandatory or optional
    for any corresponding short options.

    Report bugs to <joshua.gf.arul@gmail.com>

>The input phrase **MUST** be surrounded by quotes.

>If you run into any missing libs while trying to run, try this:
>export LD_LIBRARY_PATH=/usr/local/lib

The numeric feature stream goes to standard output, and verbose debugging output goes to stderr, so in production you will likely have to run it with `2>/dev/null`. If you don't want to prepend the LD_LIBRARY_PATH, which is necessary on redhat but not debian variants, see e.g. https://serverfault.com/a/372998

More info is documented in this [paper](assets/Spoken-English-Intelligibility-Remediation.pdf), and also [here](http://arxiv.org/abs/1709.01713).

A demo can be run using `assets/combo.dict` and `assets/featex.raw`:

    $ ./featex.o -c assets/combo.dict -i assets/featex.raw -P 'we drank tea in the afternoon and watched tv`

If you only want to see the end result and filter out all the debug data, redirect `stderr`:

    $ ./featex.o -c assets/combo.dict -i assets/featex.raw -P 'we drank tea in the afternoon and watched tv` 2> /dev/null
