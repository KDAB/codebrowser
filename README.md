This is the generator for the woboq code browser.

See https://code.woboq.org for an example.

To browse the source code of the generator using the code browser itself:
https://code.woboq.org/userspace/codebrowser/

Main page: https://woboq.com/codebrowser.html

The announcement blog: https://woboq.com/blog/codebrowser-introduction.html

[![Build Status](https://travis-ci.org/woboq/woboq_codebrowser.svg?branch=master)](https://travis-ci.org/woboq/woboq_codebrowser)

Introduction and Design
=======================

There is a pre-processing step on your code that generates static html and
reference database. The output of this phase is just a set of static files that
can be uploaded on any web hoster. No software is required on the server other
than the most basic web server that can serve files.

While generating the code, you will give to the generator an output directory.
The files reference themselves using relative path. The layout in the output
directory will look like this:
(Assuming the output directory is ~/public_html/mycode)

$OUTPUTDIR/../data/  or ~/public_html/data/
  is where all javascript and css files are located. Those are static files shipped with the code browser, they are not generated.

$OUTPUTDIR/projectname  or ~/public_html/mycode/projectname
  contains the generated html files for your project

$OUTPUTDIR/refs  or ~/public_html/mycode/refs
  contains the generated "database" used for the tooltips

$OUTPUTDIR/include  or ~/public_html/mycode/include
  contains the generated html files for the files in /usr/include


The idea is that you can have several project sharing the same output
directory. In that case they will also share references and use searches will
work between them.


Install via RPM/DEB
===================

Please look at https://woboq.com/codebrowser-download.html

Install via homebrew
====================
Please look at https://woboq.com/codebrowser-download.html

Install via Arch User Repository (AUR)
======================================

Execute these commands in Arch Linux:
```bash
git clone https://aur.archlinux.org/woboq_codebrowser-git.git
cd woboq_codebrowser-git
makepkg -si
```


Compiling the generator on Linux
================================

You need:
 - The clang libraries version 3.4 or later
 
You may want to `sudo apt install llvm-7 clang-7 libclang-7-dev` on Ubuntu if you ran into error like that clang says it cannot find "ClangConfig.cmake". More details in [issues#74](https://github.com/woboq/woboq_codebrowser/issues/74) .
 
Example:
```bash
cmake . -DCMAKE_BUILD_TYPE=Release
make
```

Compiling the generator on macOS
==============================================

Install XCode and then the command line tools:
```bash
xcode-select --install
```

Install the clang libraries via homebrew ( http://brew.sh/ ):
```bash
brew install llvm --with-clang --rtti
```

Then compile the generator:
```bash
cmake . -DCMAKE_PREFIX_PATH=/usr/local/Cellar/llvm/<your_llvm_version> -DCMAKE_BUILD_TYPE=Release
make
```

Using the generator
===================

Step 1: Generate the compile_commands.json (see chapter "Compilation Database" below) for your project
-------------------------------------------------------------------------------------------------------

The code browser is built around the clang tooling infrastructure that uses compile_commands.json
http://clang.llvm.org/docs/JSONCompilationDatabase.html

See the section "Compilation Database (compile_commands.json)" below.

Step 2: Create code HTML using codebrowser_generator
------------------------------------------------------------

Before generating, make sure the output directory is empty or does not contains
stale files from a previous generation.

Call the codebrowser_generator. See later for argument specification

Step 3: Generate the directory index HTML files using codebrowser_indexgenerator
------------------------------------------------------------------------------------------

By running the codebrowser_indexgenerator with the output directory as an argument

Step 4: Copy the static data/ directory one level above the generated html
---------------------------------------------------------------------------

Step 5: Open it in a browser or upload it to your webserver
------------------------------------------------------------

Note: By default, browsers do not allow AJAX on `file://` for security reasons. 
You need to upload the output directory on a web server, or serve your files with a local apache or nginx server. 
Alternatively, you can disable that security in Firefox by setting security.fileuri.strict_origin_policy to false in about:config (http://kb.mozillazine.org/Security.fileuri.strict_origin_policy) or start Chrome with the [--allow-file-access-from-files](http://www.chrome-allow-file-access-from-file.com/) option.

Full usage example
==================

Let's be meta in this example and try to generate the HTML files for the code browser itself.
Assuming you are in the cloned directory:

```bash
OUTPUT_DIRECTORY=~/public_html/codebrowser
DATA_DIRECTORY=$OUTPUT_DIRECTORY/../data
BUILD_DIRECTORY=$PWD
SOURCE_DIRECTORY=$PWD
VERSION=`git describe --always --tags`
cmake . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
./generator/codebrowser_generator -b $BUILD_DIRECTORY -a -o $OUTPUT_DIRECTORY -p codebrowser:$SOURCE_DIRECTORY:$VERSION
./indexgenerator/codebrowser_indexgenerator $OUTPUT_DIRECTORY
cp -rv ./data $DATA_DIRECTORY
```

You can adjust the variables and try similar commands to generate other projects.


Arguments to codebrowser_generator
==================================

Compiles sources into HTML files

```bash
codebrowser_generator -a -o <output_dir> -b <buld_dir> -p <projectname>:<source_dir>[:<revision>] [-d <data_url>] [-e <remote_path>:<source_dir>:<remote_url>]
```

 -a process all files from the compile_commands.json.  If this argument is not
    passed, the list of files to process need to be passed

 -o with the output directory where the generated files will be put

 -b the "build directory" containing the compile_commands.json If this argument
    is not passed, the compilation arguments can be passed on the command line
    after  --

 -p (one or more) with project specification. That is the name of the project,
    the absolute path of the source code, and the revision separated by colons
    example: -p projectname:/path/to/source/code:0.3beta

 -d specify the data url where all the javascript and css files are found.
    default to ../data relative to the output dir
    example: -d https://code.woboq.org/data

 -e reference to an external project.
    example:-e clang/include/clang:/opt/llvm/include/clang/:https://code.woboq.org/llvm


Arguments to codebrowser_indexgenerator
=======================================

Generates index HTML files for each directory for the generated HTML files

```bash
codebrowser_indexgenerator <output_dir> [-d data_url] [-p project_definition]
```

 -p (one or more) with project specification. That is the name of the project,
    the absolute path of the source code, and the revision separated by colons
    example: -p projectname:/path/to/source/code:0.3beta

 -d specify the data url where all the javascript and css files are found.
    default to ../data relative to the output dir
    example: -d https://code.woboq.org/data



Compilation Database (compile_commands.json)
============================================
The generator is a tool which uses clang's LibTooling. It needs either a
compile_commands.json or the arguments to be passed after '--' if they are
the same for every file.

To generate the compile_commands.json:
* For cmake, pass -DCMAKE_EXPORT_COMPILE_COMMANDS=ON as a cmake parameter
* For qmake, configure/autoconf and others, follow the instructions in scripts/fake_compiler.sh or scripts/woboq_cc.js.
These are fake compilers that append the compiler invocation to the json file and forward to the real compiler.
Your real compiler is overriden using the CC/CXX environment variables
Make sure to have the json file properly terminated.
* If you use ninja, you can use ninja -t compdb
* If you use qbs, you can use qbs generate --generator clangdb
* There is also a project called Build EAR (Bear) that achieves a similar thing as our fake compilers
but is using LD_PRELOAD to inject itself into the build process to catch how the compiler is invoked.
https://github.com/rizsotto/Bear

There is also some further information on https://sarcasm.github.io/notes/dev/compilation-database.html#clang


Getting help
============
No matter if you are a licensee or are just curious and evaulating, we'd love to help you.
Ask us via e-mail on contact@woboq.com
Or on IRC in #woboq on irc.freenode.net

If you find a bug or incompatibility, please file a github issue:
https://github.com/woboq/woboq_codebrowser/issues


Licence information
====================
Licensees holding valid commercial licenses provided by Woboq may use
this software in accordance with the terms contained in a written agreement
between the licensee and Woboq.
For further information see https://woboq.com/codebrowser.html

Alternatively, this work may be used under a Creative Commons
Attribution-NonCommercial-ShareAlike 3.0 (CC-BY-NC-SA 3.0) License.
http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US

This license does not allow you to use the code browser to assist the
development of your commercial software. If you intent to do so, consider
purchasing a commercial licence.
