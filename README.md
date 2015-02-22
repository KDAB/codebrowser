This is the generator for the woboq code browser.

See http://code.woboq.org for an example
See http://woboq.com/codebrowser.html for license information
See also the announcement http://woboq.com/blog/codebrowser-introduction.html

Browse the source code of the generator using the code browser itself:
http://code.woboq.org/userspace/codebrowser/

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
  is where all javascript and css files are located.

$OUTPUTDIR/projectname  or ~/public_html/mycode/projectname
  contains the generated html files for your project

$OUTPUTDIR/refs  or ~/public_html/mycode/refs
  contains the "database" used for the tooltips

$OUTPUTDIR/include  or ~/public_html/mycode/include
  contains the generated files for the files in /usr/include


The idea is that you can have several project sharing the same output
directory. In that case they will also share references and use searches will
work between them.



Compiling the generator on Linux
================================

You need:
 - The clang libraries version 3.3 or later

Example:
```bash
cmake . -DLLVM_CONFIG_EXECUTABLE=/opt/llvm/bin/llvm-config -DCMAKE_CXX_COMPILER=/opt/llvm/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release
make
# because clang searches for includes files in ../lib relative to the executable: 
ln -s /opt/llvm/lib/ .
```


If you have errors related to exceptions, edit generator/CMakeLists.txt and replace -fno-rtti by -fexceptions

Compiling the generator on OS X 10.9 Mavericks
==============================================

You need:
 - The clang libraries, for example in version 3.3
 - XCode 5.x and the command line tools and includes

Install XCode and then the command line tools:
```bash
xcode-select --install
```

Install the clang libraries via homebrew ( http://brew.sh/ ):
```bash
brew tap homebrew/versions
brew install -vd llvm33  --with-libcxx --with-clang --rtti
```

Then compile the generator:
```bash
cmake . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLLVM_CONFIG_EXECUTABLE=/usr/local//bin/llvm-config-3.3  -DCMAKE_BUILD_TYPE=Release
make
# because clang searches for includes files in ../lib relative to the executable:
ln -s /opt/local/lib/ ./lib
```


Using the generator
===================

Step 1: Generate the compile_commands.json for your project

The code browser is built around the clang tooling infrastructure that uses compile_commands.json
http://clang.llvm.org/docs/JSONCompilationDatabase.html
If your build system is cmake, just pass -DCMAKE_EXPORT_COMPILE_COMMANDS=ON to cmake to generate the script.
For other build systems (e.g. qmake, make) you can use scripts/fake_compiler.sh as compiler (see comments in that file)

Step 2: Create code HTML using codebrowser_generator

Before generating, make sure the output directory is empty or does not contains
stale files from a previous generation.

Call the codebrowser_generator. See later for argument specification

Step 3: Generate the index HTML files using codebrowser_indexer

By running the codebrowser_indexer with the output directory as an argument

Step 4: Copy the data/ directory one level above the generated html

Example:
To generate the code for this project itself:

```bash
cmake . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
./generator/codebrowser_generator -b $PWD -a -o ~/public_html/codebrowser -p codebrowser:$PWD:`git describe --always --tags`
./indexgenerator/codebrowser_indexgenerator ~/public_html/codebrowser
ln -s ./data ~/public_html/
```

Step 5: Open it in a browser or upload it to your webserver

Note: By default, browsers do not allow AJAX on `file://` for security reasons. 
You need to upload the output directory on a web server, or serve your files with a local apache or nginx server. 
Alternatively, you can disable that security in Firefox by setting security.fileuri.strict_origin_policy to false in about:config (http://kb.mozillazine.org/Security.fileuri.strict_origin_policy) or start Chrome with the [--allow-file-access-from-files](http://www.chrome-allow-file-access-from-file.com/) option.

Arguments to codebrowser_generator
==================================

```bash
codebrowser_generator -a -o <output_dir> -b <buld_dir> -p <projectname>:<source_dir>[:<revision>] [-d <data_url>] [-e <remote_path>:<source_dir>:<remote_url>]
```

 -a process all files from the compile_commands.json.  If this argument is not
    passed, the list of files to process need to be passed

 -o with the output directory where the generated files will be put

 -b the "build directory" containing the compile_commands.json

 -p (one or more) with project specification. That is the name of the project,
    the absolute path of the source code, and the revision separated by colons
    example: -p projectname:/path/to/source/code:0.3beta

 -d specify the data url where all the javascript and css files are found.
    default to ../data relative to the output dir
    example: -d http://code.woboq.org/data

 -e reference to an external project.
    example:-e llvm/tools/clang/include/clang:/opt/llvm/include/clang/:http://code.woboq.org/userspace

Licence information:
====================
Licensees holding valid commercial licenses provided by Woboq may use
this software in accordance with the terms contained in a written agreement
between the licensee and Woboq.
For further information see http://woboq.com/codebrowser.html

Alternatively, this work may be used under a Creative Commons
Attribution-NonCommercial-ShareAlike 3.0 (CC-BY-NC-SA 3.0) License.
http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US
This license does not allow you to use the code browser to assist the
development of your commercial software. If you intent to do so, consider
purchasing a commercial licence.
