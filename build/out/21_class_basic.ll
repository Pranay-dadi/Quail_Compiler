; ModuleID = 'quail'
source_filename = "quail"

%Box = type { i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Box_getValue(%Box* nocapture readonly %this_arg) local_unnamed_addr #0 {
entry:
  %this.value.ptr = getelementptr %Box, %Box* %this_arg, i64 0, i32 0
  %value = load i32, i32* %this.value.ptr, align 4
  ret i32 %value
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @Box_setValue(%Box* nocapture writeonly %this_arg, i32 %0) local_unnamed_addr #1 {
entry:
  %this.value.ptr = getelementptr %Box, %Box* %this_arg, i64 0, i32 0
  store i32 %0, i32* %this.value.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #2 {
entry:
  ret i32 42
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #1 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #2 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
