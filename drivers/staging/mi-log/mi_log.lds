SECTIONS {
	__patchable_function_entries : { *(__patchable_function_entries) }
	.text : {
		init_module.cfi_jt = . + 0x270;
		delay_read.cfi_jt = . + 0x278;
		cleanup_module.cfi_jt = . + 0x280;
	}
}
