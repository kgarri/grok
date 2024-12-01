; ModuleID = "main"
target triple = "x86_64-w64-windows-gnu"
target datalayout = ""

define i32 @"test"()
{
test_entry:
  %".2" = srem i32 69, 60
  ret i32 %".2"
}

define i32 @"main"()
{
main_entry:
  %".2" = call i32 @"test"()
  ret i32 %".2"
}
