; ModuleID = "main"
target triple = "x86_64-unknown-linux-gnu"
target datalayout = ""

@"true" = constant i1 1
@"false" = global i1 0
define i32 @"testFunction"()
{
testFunction_entry:
  ret i32 5
}

define i32 @"add"(i32 %".1", i32 %".2")
{
add_entry:
  %".4" = alloca i32
  store i32 %".1", i32* %".4"
  %".6" = alloca i32
  store i32 %".2", i32* %".6"
  %".8" = load i32, i32* %".4"
  %".9" = load i32, i32* %".6"
  %".10" = add i32 %".8", %".9"
  ret i32 %".10"
}

define i32 @"mul"(i32 %".1", i32 %".2")
{
mul_entry:
  %".4" = alloca i32
  store i32 %".1", i32* %".4"
  %".6" = alloca i32
  store i32 %".2", i32* %".6"
  %".8" = load i32, i32* %".4"
  %".9" = load i32, i32* %".6"
  %".10" = mul i32 %".8", %".9"
  ret i32 %".10"
}

define i32 @"fib"(i32 %".1")
{
fib_entry:
  %".3" = alloca i32
  store i32 %".1", i32* %".3"
  %".5" = load i32, i32* %".3"
  %".6" = icmp sle i32 %".5", 1
  br i1 %".6", label %"fib_entry.if", label %"fib_entry.endif"
fib_entry.if:
  ret i32 1
fib_entry.endif:
  %".9" = load i32, i32* %".3"
  %".10" = sub i32 %".9", 1
  %".11" = call i32 @"fib"(i32 %".10")
  %".12" = load i32, i32* %".3"
  %".13" = sub i32 %".12", 2
  %".14" = call i32 @"fib"(i32 %".13")
  %".15" = add i32 %".11", %".14"
  ret i32 %".15"
}

define i32 @"main"()
{
main_entry:
  %".2" = bitcast [7 x i8]* @"Hello _string" to i8*
  %".3" = alloca i8*
  store i8* %".2", i8** %".3"
  %".5" = bitcast [7 x i8]* @"World!_string" to i8*
  %".6" = alloca i8*
  store i8* %".5", i8** %".6"
  %".8" = load i8*, i8** %".3"
  %".9" = load i8*, i8** %".6"
  %".10" = call i8* (i8*, ...) @"concat"(i8* %".8", i8* %".9")
  %".11" = bitcast [3 x i8]* @"fstr" to i8*
  %".12" = call i32 (i8*, ...) @"printf"(i8* %".11", i8* %".10")
  %".13" = call i32 @"testFunction"()
  %".14" = alloca i32
  store i32 %".13", i32* %".14"
  %".16" = alloca i32
  store i32 9, i32* %".16"
  %".18" = load i32, i32* %".14"
  %".19" = icmp sge i32 %".18", 5
  br i1 %".19", label %"main_entry.if", label %"main_entry.else"
main_entry.if:
  store i32 15, i32* %".14"
  br label %"main_entry.endif"
main_entry.else:
  store i32 1, i32* %".14"
  br label %"main_entry.endif"
main_entry.endif:
  %".25" = load i32, i32* %".14"
  %".26" = call i32 @"fib"(i32 %".25")
  ret i32 %".26"
}

@"Hello _string" = internal constant [7 x i8] c"Hello \00"
@"World!_string" = internal constant [7 x i8] c"World!\00"
declare i8* @"concat"(i8* %".1", ...)

@"fstr" = internal constant [3 x i8] c"%s\0a"
declare i32 @"printf"(i8* %".1", ...)
