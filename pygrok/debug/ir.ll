; ModuleID = "main"
target triple = "x86_64-unknown-linux-gnu"
target datalayout = ""

define i32 @"testFunction"()
{
testFunction_entry:
  ret i32 1
}

define i32 @"main"()
{
main_entry:
  %".2" = alloca [13 x i8]
  store [13 x i8] c"Hello world!\00", [13 x i8]* %".2"
  %".4" = call i32 @"testFunction"()
  ret i32 1
}

@"Hello world!_string" = private global [13 x i8] c"Hello world!\00"