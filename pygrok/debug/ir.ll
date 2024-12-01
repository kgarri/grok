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
  %".4" = load i32, i32* %".2"
  %".5" = icmp eq i32 %".4", 4
  br i1 %".5", label %"main_entry.if", label %"main_entry.else"
main_entry.if:
  %".7" = load i32, i32* %".2"
  %".8" = mul i32 %".7", 2
  store i32 %".8", i32* %".2"
  br label %"main_entry.endif"
main_entry.else:
  %".11" = load i32, i32* %".2"
  %".12" = icmp eq i32 %".11", 3
  br i1 %".12", label %"main_entry.else.if", label %"main_entry.else.else"
main_entry.endif:
  %".23" = load i32, i32* %".2"
  ret i32 %".23"
main_entry.else.if:
  %".14" = load i32, i32* %".2"
  %".15" = mul i32 %".14", 3
  store i32 %".15", i32* %".2"
  br label %"main_entry.else.endif"
main_entry.else.else:
  %".18" = load i32, i32* %".2"
  %".19" = sdiv i32 %".18", 2
  store i32 %".19", i32* %".2"
  br label %"main_entry.else.endif"
main_entry.else.endif:
  br label %"main_entry.endif"
}
