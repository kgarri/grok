; ModuleID = "main"
target triple = "x86_64-unknown-linux-gnu"
target datalayout = ""

@"true" = constant i1 1
@"false" = global i1 0
define i32 @"main"()
{
main_entry:
  %".2" = alloca i32
  store i32 3, i32* %".2"
  %".4" = alloca i32
  store i32 4, i32* %".4"
  %".6" = or i1 1, 0
  br i1 %".6", label %"main_entry.if", label %"main_entry.else"
main_entry.if:
  ret i32 1
main_entry.else:
  ret i32 0
main_entry.endif:
  ret i32 0
}
