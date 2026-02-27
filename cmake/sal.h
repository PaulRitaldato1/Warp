// Minimal SAL (Source Annotation Language) stub for non-MSVC / non-Windows platforms.
// DirectXMath uses SAL annotations purely as documentation — they carry no runtime semantics.
#pragma once

#ifndef _SAL_H_
#define _SAL_H_

// Input annotations
#define _In_
#define _In_z_
#define _In_opt_
#define _In_opt_z_
#define _In_reads_(n)
#define _In_reads_opt_(n)
#define _In_reads_bytes_(n)
#define _In_reads_bytes_opt_(n)
#define _In_range_(lo,hi)

// Output annotations
#define _Out_
#define _Out_opt_
#define _Out_writes_(n)
#define _Out_writes_opt_(n)
#define _Out_writes_bytes_(n)
#define _Out_writes_bytes_opt_(n)
#define _Out_writes_all_(n)
#define _Out_writes_all_opt_(n)

// In-Out annotations
#define _Inout_
#define _Inout_opt_
#define _Inout_updates_(n)
#define _Inout_updates_opt_(n)
#define _Inout_updates_bytes_(n)
#define _Inout_updates_bytes_opt_(n)

// Return value annotations
#define _Ret_maybenull_
#define _Ret_notnull_
#define _Ret_valid_
#define _Ret_writes_(n)
#define _Ret_writes_maybenull_(n)

// Other common annotations
#define _Check_return_
#define _Must_inspect_result_
#define _Success_(x)
#define _Pre_valid_
#define _Post_valid_
#define _Pre_satisfies_(x)
#define _Post_satisfies_(x)
#define _Outptr_
#define _Outptr_opt_
#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _Deref_out_
#define _Deref_out_opt_
#define _Notnull_
#define _Maybenull_
#define _Null_terminated_
#define _NullNull_terminated_
#define _Printf_format_string_

// SAL 2 annotations (used in newer headers)
#define _Field_size_(n)
#define _Field_size_opt_(n)
#define _Field_size_bytes_(n)
#define _Field_size_bytes_opt_(n)
#define _Field_z_
#define _Struct_size_bytes_(n)
#define _Readable_bytes_(n)
#define _Writable_bytes_(n)
#define _At_(x,y)
#define _When_(x,y)
#define _Analysis_assume_(x)

// Function-definition annotation: use annotations from declaration
#define _Use_decl_annotations_

#endif // _SAL_H_
