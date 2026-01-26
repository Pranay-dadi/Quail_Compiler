; ModuleID = 'Quail_Compiler'
source_filename = "Quail_Compiler"

define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 0, i32* %a, align 4
  %c = alloca i32, align 4
  store i32 0, i32* %c, align 4
  %c1 = load i32, i32* %c, align 4
  ret i32 0
}
