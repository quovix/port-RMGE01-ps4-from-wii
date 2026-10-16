/* @orig none — config de build (quovix). Link script do eboot engine-jump,
   baseado no link.x do OpenOrbis PS4Toolchain (sample permissivo), com UMA
   mudança: os construtores estáticos globais do MOTOR são separados do
   .init_array (que alimenta DT_INIT_ARRAY, rodado pelo loader antes do main)
   para uma seção .engine_init_array que o loader NÃO roda. Assim nosso main()
   ganha controle antes deles e pode iterá-los com log, pra localizar qual
   construtor crasha. Os construtores do HARNESS (nossos) seguem no .init_array
   e rodam normalmente antes do main. */
SECTIONS
{
	.text : ALIGN(0x4000) {
		__text_start = .;
		QUAD(0x6365786562696C2F);
		QUAD(0x2E666C652D646C2F);
		QUAD(0x00000000312E6F73);
		QUAD(0x0000000000000000);
		*(.text .text.*)
	}

	.rodata : ALIGN(0x10) {
		*(.rodata .rodata.*)
	}

	.eh_frame : {
	   __eh_frame_start = .;
	   KEEP(*(.eh_frame))
	   __eh_frame_end = .;
	}

	.eh_frame_hdr : {
	   __eh_frame_hdr_start = .;
	   KEEP(*(.eh_frame_hdr))
	   __eh_frame_hdr_end = .;
	}

	.data.rel.ro : ALIGN(0x4000) {
	   __data_relro_start = .;
	   KEEP(*(.data.rel.ro .data.rel.ro.*))
	   __data_relro_start = .;
	}

	/* Construtores do HARNESS (nossos): alimentam DT_INIT_ARRAY, rodam antes do
	   main. Os do MOTOR foram renomeados p/ .engine_init_array via objcopy no
	   build (não casam aqui), então só sobram os nossos. */
	.init_array : {
		*(.init_array .init_array.*);
	}

	.dynamic : {
		*(.dynamic);
	}

	.tls : {
		*(.tdata);
		*(.tbss);
	}

	.got : ALIGN(SIZEOF(.data.rel.ro) > 0 ? 8 : 0x4000) {
		*(.got)
	}

	.got.plt : ALIGN((SIZEOF(.got) > 0 || SIZEOF(.data.rel.ro) > 0) ? 8 : 0x4000) {
		*(.got.plt)
	}

	.data.sce_process_param : ALIGN(0x4000) {
		KEEP(*(.data.sce_process_param))
	}

	.data.sce_module_param : ALIGN(0x4000) {
		KEEP(*(.data.sce_module_param))
	}

	/* Construtores do MOTOR: renomeados p/ .engine_init_array via objcopy no
	   build. Seção própria (RW, fora do relro) que NÃO alimenta DT_INIT_ARRAY
	   -> o loader não roda. main() os itera com log (crash-driven). */
	.engine_init_array : {
		__qvx_einit_start = .;
		KEEP(*(.engine_init_array .engine_init_array.*))
		__qvx_einit_end = .;
	}

	.data : {
		*(.data)
	}

	.bss : {
		*(.bss .bss.*)
	}

	/DISCARD/ : {
		QUAD(_GLOBAL_OFFSET_TABLE_)
	}
}
