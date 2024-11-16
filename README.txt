Grok, like its namesake, is meant to be a simple and intuitive programing language
that even the newest programmer should be able to pick up fairly quickly. This allows
programmers to spend more time programming and less time looking up syntax or 
trying to remember the quirks of a specific language. 
This project is a CSC-333 (Programming Language Theory) assignment.

-----------------------------------------------------------------------------------------------------------------------------
=============================================================================================================================
-----------------------------------------------------------------------------------------------------------------------------
To run on Windows: 
1. Have LLVM and Clang++ installed (installation with Msys2 package manager is easiest) 
2. Open Msys2 MinGW64 terminal 
3. Run the following command in the Grok directory to compile to k.exe: 
  clang++ -v -g main.cpp lexer.cpp parser.cpp codegen.cpp toplevel.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native` -fuse-ld=lld -o k
4. Use this command to run: 
  start k.exe
