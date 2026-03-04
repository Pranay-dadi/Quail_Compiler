; ModuleID = 'quail'
source_filename = "quail"

%Rectangle = type { i32, i32 }
%Circle = type { i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @Rectangle_init(%Rectangle* nocapture writeonly %this_arg, i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %this.width.ptr = getelementptr %Rectangle, %Rectangle* %this_arg, i64 0, i32 0
  store i32 %0, i32* %this.width.ptr, align 4
  %this.height.ptr = getelementptr %Rectangle, %Rectangle* %this_arg, i64 0, i32 1
  store i32 %1, i32* %this.height.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Rectangle_area(%Rectangle* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.width.ptr = getelementptr %Rectangle, %Rectangle* %this_arg, i64 0, i32 0
  %width = load i32, i32* %this.width.ptr, align 4
  %this.height.ptr = getelementptr %Rectangle, %Rectangle* %this_arg, i64 0, i32 1
  %height = load i32, i32* %this.height.ptr, align 4
  %mul = mul i32 %height, %width
  ret i32 %mul
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Rectangle_perimeter(%Rectangle* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.width.ptr = getelementptr %Rectangle, %Rectangle* %this_arg, i64 0, i32 0
  %width = load i32, i32* %this.width.ptr, align 4
  %this.height.ptr = getelementptr %Rectangle, %Rectangle* %this_arg, i64 0, i32 1
  %height = load i32, i32* %this.height.ptr, align 4
  %add = add i32 %height, %width
  %mul = shl i32 %add, 1
  ret i32 %mul
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @Circle_setRadius(%Circle* nocapture writeonly %this_arg, i32 %0) local_unnamed_addr #0 {
entry:
  %this.radius.ptr = getelementptr %Circle, %Circle* %this_arg, i64 0, i32 0
  store i32 %0, i32* %this.radius.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Circle_getRadius(%Circle* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.radius.ptr = getelementptr %Circle, %Circle* %this_arg, i64 0, i32 0
  %radius = load i32, i32* %this.radius.ptr, align 4
  ret i32 %radius
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #2 {
entry:
  ret i32 12
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #2 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
