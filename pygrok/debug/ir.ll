; ModuleID = "main"
target triple = "x86_64-w64-windows-gnu"
target datalayout = ""

define i32 @"test"(i32 %".1", i32 %".2")
{
test_entry:
  %".4" = alloca i32
  store i32 %".1", i32* %".4"
  %".6" = alloca i32
  store i32 %".2", i32* %".6"
  %".8" = load i32, i32* %".4"
  %".9" = load i32, i32* %".6"
  %".10" = add i32 %".8", %".9"
  ret i32 %".10"
}

define float @"mult"(float %".1", float %".2")
{
mult_entry:
  %".4" = alloca float
  store float %".1", float* %".4"
  %".6" = alloca float
  store float %".2", float* %".6"
  %".8" = load float, float* %".4"
  %".9" = load float, float* %".6"
  %".10" = fadd float %".8", %".9"
  ret float %".10"
}

define i32 @"main"()
{
main_entry:
  %".2" = call i32 @"test"(i32 3, i32 4)
  ret i32 %".2"
}
