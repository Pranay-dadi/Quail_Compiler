; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @add(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %add = add i32 %1, %0
  ret i32 %add
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @multiply(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %mul = mul i32 %1, %0
  ret i32 %mul
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @square(i32 %0) local_unnamed_addr #0 {
entry:
  %mul.i = mul i32 %0, %0
  ret i32 %mul.i
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 25
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
