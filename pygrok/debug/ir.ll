; ModuleID = "main"
target triple = "x86_64-w64-windows-gnu"
target datalayout = ""

define i32 @"main"()
{
main_entry:
  %".2" = alloca i32
  store i32 5, i32* %".2"
  %".4" = load i32, i32* %".2"
  ret i32 %".4"
}
