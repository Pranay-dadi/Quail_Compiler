; ModuleID = 'quail'
source_filename = "quail"

define i32 @main() {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i32, align 4
  store i32 10, i32* %a, align 4
  store i32 20, i32* %b, align 4
  %a1 = load i32, i32* %a, align 4
  %b2 = load i32, i32* %b, align 4
  %0 = mul i32 %b2, 2
  %1 = add i32 %a1, %0
  store i32 %1, i32* %c, align 4
  %c3 = load i32, i32* %c, align 4
  ret i32 %c3
}
