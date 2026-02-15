; ModuleID = 'quail'
source_filename = "quail"

define i32 @main() {
entry:
  %i = alloca i32, align 4
  %sum = alloca i32, align 4
  store i32 0, i32* %sum, align 4
  %i1 = load i32, i32* %i, align 4
  %0 = icmp sle i32 %i1, 20
  ret i32 0
}
