; ModuleID = 'quail'
source_filename = "quail"

@.str = private unnamed_addr constant [23 x i8] c"=== Quail I/O Demo ===\00", align 1
@.fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@.str.2 = private unnamed_addr constant [9 x i8] c"Name  : \00", align 1
@.str.3 = private unnamed_addr constant [14 x i8] c"Quail Student\00", align 1
@.str.4 = private unnamed_addr constant [9 x i8] c"Age   : \00", align 1
@.fmt.5 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@.str.6 = private unnamed_addr constant [9 x i8] c"GPA   : \00", align 1
@.fmt.7 = private unnamed_addr constant [3 x i8] c"%g\00", align 1
@.str.8 = private unnamed_addr constant [17 x i8] c"Score (doubled):\00", align 1
@.str.9 = private unnamed_addr constant [15 x i8] c"Age next year:\00", align 1
@.str.10 = private unnamed_addr constant [15 x i8] c"GPA floored  :\00", align 1
@.str.11 = private unnamed_addr constant [10 x i8] c"Tab demo:\00", align 1
@.str.12 = private unnamed_addr constant [27 x i8] c"Column A\09Column B\09Column C\00", align 1
@.str.13 = private unnamed_addr constant [7 x i8] c"Pass? \00", align 1
@.str.14 = private unnamed_addr constant [4 x i8] c"YES\00", align 1
@.str.16 = private unnamed_addr constant [13 x i8] c"=== Done ===\00", align 1

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @double_it(i32 %0) local_unnamed_addr #0 {
entry:
  %mul = shl i32 %0, 1
  ret i32 %mul
}

; Function Attrs: nofree nounwind
define i32 @main() local_unnamed_addr #1 {
ifcont:
  %0 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str, i64 0, i64 0))
  %putchar = tail call i32 @putchar(i32 10)
  %putchar12 = tail call i32 @putchar(i32 10)
  %1 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.2, i64 0, i64 0))
  %2 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str.3, i64 0, i64 0))
  %putchar13 = tail call i32 @putchar(i32 10)
  %3 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.4, i64 0, i64 0))
  %4 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.5, i64 0, i64 0), i32 20)
  %putchar14 = tail call i32 @putchar(i32 10)
  %5 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.6, i64 0, i64 0))
  %6 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.7, i64 0, i64 0), double 3.850000e+00)
  %putchar15 = tail call i32 @putchar(i32 10)
  %7 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([17 x i8], [17 x i8]* @.str.8, i64 0, i64 0))
  %8 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.5, i64 0, i64 0), i32 42)
  %putchar16 = tail call i32 @putchar(i32 10)
  %9 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.9, i64 0, i64 0))
  %10 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.5, i64 0, i64 0), i32 21)
  %putchar17 = tail call i32 @putchar(i32 10)
  %11 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.10, i64 0, i64 0))
  %12 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.5, i64 0, i64 0), i32 2)
  %putchar18 = tail call i32 @putchar(i32 10)
  %putchar19 = tail call i32 @putchar(i32 10)
  %13 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.11, i64 0, i64 0))
  %putchar20 = tail call i32 @putchar(i32 10)
  %14 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([27 x i8], [27 x i8]* @.str.12, i64 0, i64 0))
  %putchar21 = tail call i32 @putchar(i32 10)
  %putchar22 = tail call i32 @putchar(i32 10)
  %15 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.13, i64 0, i64 0))
  %16 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.14, i64 0, i64 0))
  %putchar26 = tail call i32 @putchar(i32 10)
  %putchar24 = tail call i32 @putchar(i32 10)
  %17 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.16, i64 0, i64 0))
  %putchar25 = tail call i32 @putchar(i32 10)
  ret i32 42
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @putchar(i32 noundef) local_unnamed_addr #1

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
attributes #1 = { nofree nounwind }
