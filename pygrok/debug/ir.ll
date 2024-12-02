; ModuleID = "main"
target triple = "x86_64-w64-windows-gnu"
target datalayout = ""

define i32 @"testFunction"()
{
testFunction_entry:
  ret i32 1
}

define i32 @"main"()
{
main_entry:
  %".2" = bitcast [13 x i8] @"str_const" to i8*
  %".3" = alloca i8*
  store i8* %".2", i8** %".3"
  %".5" = call i32 @"testFunction"()
  ret i32 1
}
