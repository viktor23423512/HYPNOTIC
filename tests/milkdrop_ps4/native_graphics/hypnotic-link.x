/* SPDX-License-Identifier: GPL-3.0-only
 * Derived from the installed OpenOrbis PS4Toolchain/link.x.
 * HYPNOTIC changes: collect suffixed TLS sections and align/pad TLS to 32
 * bytes, matching create-fself output. Original SDK remains unchanged.
 * Upstream: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
 * License text: third_party/OpenOrbis-LICENSE.txt
 */
SECTIONS
{
	# Combine text, rodata, and eh_frame stuff
	# In case you'll ever need to get the base address of eboot.bin, you can use the &__text_start symbol.
	# No need to go through libkernel for that.
	.text : ALIGN(0x4000) {
		__text_start = .;
		# "/libexec/ld-elf.so.1"
		QUAD(0x6365786562696C2F);
		QUAD(0x2E666C652D646C2F);
		QUAD(0x00000000312E6F73);
		QUAD(0x0000000000000000);

		# original .text
		*(.text .text.*)
	}

	.rodata : ALIGN(0x10) {
		*(.rodata .rodata.*)
	}

    # Since we lack a proper POSIX dladdr() on a PS4, we define these global symbols
    # For libunwind's bare metal compilation mode.
    # That's way better than going through libkernel... ;-;
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
	
	.init_array : {
		*(.init_array);
	}
	
	.dynamic : {
		*(.dynamic);
	}

	.tls : ALIGN(32) {
		*(.tdata .tdata.*);
		*(.tbss .tbss.*);
        . = ALIGN(32);
	}

	# Align .got to 0x4000 if .data.rel.ro doesn't exist
	.got : ALIGN(SIZEOF(.data.rel.ro) > 0 ? 8 : 0x4000) {
		*(.got)
	}

	# Align .got.plt to 0x4000 if .got doesn't exist
	.got.plt : ALIGN((SIZEOF(.got) > 0 || SIZEOF(.data.rel.ro) > 0) ? 8 : 0x4000) {
		*(.got.plt)
	}

	.data.sce_process_param : ALIGN(0x4000) {
		KEEP(*(.data.sce_process_param))
	}

	.data.sce_module_param : ALIGN(0x4000) {
		KEEP(*(.data.sce_module_param))
	}

	.data : {
		*(.data)
	}

	.bss : {
		*(.bss .bss.*)
	}

	# Force .got.plt to appear, because SPRX requires a valid .got.plt.
	/DISCARD/ : {
		QUAD(_GLOBAL_OFFSET_TABLE_)
	}
}
