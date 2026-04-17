; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: mustprogress nofree nosync nounwind readnone willreturn
define i32 @abs(i32 %0) local_unnamed_addr #0 {
entry:
  %1 = tail call i32 @llvm.abs.i32(i32 %0, i1 false)
  ret i32 %1
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #1 {
entry:
  ret i32 15
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i32 @llvm.abs.i32(i32, i1 immarg) #2

attributes #0 = { mustprogress nofree nosync nounwind readnone willreturn }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
attributes #2 = { nofree nosync nounwind readnone speculatable willreturn }
