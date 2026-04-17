; ModuleID = 'quail'
source_filename = "quail"

%Point = type { i32, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Point_getX(%Point* nocapture readonly %this_arg) local_unnamed_addr #0 {
entry:
  %this.x.ptr = getelementptr %Point, %Point* %this_arg, i64 0, i32 0
  %x = load i32, i32* %this.x.ptr, align 4
  ret i32 %x
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Point_getY(%Point* nocapture readonly %this_arg) local_unnamed_addr #0 {
entry:
  %this.y.ptr = getelementptr %Point, %Point* %this_arg, i64 0, i32 1
  %y = load i32, i32* %this.y.ptr, align 4
  ret i32 %y
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Point_area(%Point* nocapture readonly %this_arg) local_unnamed_addr #0 {
entry:
  %this.x.ptr = getelementptr %Point, %Point* %this_arg, i64 0, i32 0
  %x = load i32, i32* %this.x.ptr, align 4
  %this.y.ptr = getelementptr %Point, %Point* %this_arg, i64 0, i32 1
  %y = load i32, i32* %this.y.ptr, align 4
  %mul = mul i32 %y, %x
  ret i32 %mul
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #1 {
entry:
  ret i32 30
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
