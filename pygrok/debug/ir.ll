; ModuleID = "main"
target triple = "x86_64-w64-windows-gnu"
target datalayout = ""

define i32 @"main"()
{
main_entry:
  %".2" = srem float 0x4010000000000000, 0x4014000000000000
  ret i32 69
}
