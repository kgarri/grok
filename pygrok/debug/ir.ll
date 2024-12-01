; ModuleID = "main"
target triple = "x86_64-unknown-linux-gnu"
target datalayout = ""

define i32 @"main"()
{
main_entry:
  %".2" = alloca i32
  store i32 10, i32* %".2"
  %".4" = alloca i32
  store i32 15, i32* %".4"
  %".6" = alloca float
  store float 0x4036000000000000, float* %".6"
  ret i32 69
}
