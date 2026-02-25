; ModuleID = 'quail'
source_filename = "quail"

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @max(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %2 = icmp sgt i32 %0, %1
  %common.ret.op = select i1 %2, i32 %0, i32 %1
  ret i32 %common.ret.op
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #0 {
entry:
  ret i32 42
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
