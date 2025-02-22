rule HUNTING_implantjs_strings_1 {
    meta:
        desc = "Hunt for strings in implant.js client implant"
        author = "captainGeech"
        ref = "https://github.com/captainGeech42/implant.js"
        rev = 1
    strings:
        $s1 = "Debugger.scriptParsed"
        $s2 = "exceptionDetails"
        $s3 = "breakpointId"
        $s4 = "Could not resolve breakpoint"
        $s5 = "Debugger.setPauseOnExceptions"
        $s6 = "Runtime.evaluate"
        $s7 = "invalid arguments to ctx.output()"
        $s8 = "invalid arguments to ctx.mem.equal()"
        $s9 = "failed to parse return string to v8"
        $s10 = "couldn't resolve function %s in library %s"
        $s11 = "an invalid TYPE_POINTER value was provided at idx %d"
    condition:
        (
            (uint32be(0) == 0x7f454c46) or
            (uint16be(0) == 0x4d5a and uint16be(uint32(0x3c)) == 0x5045)
        ) and
        filesize > 10MB and
        7 of them
}

rule HUNTING_implantjs_symbols_1 {
    meta:
        desc = "Hunt for symbols in implant.js client implant"
        author = "captainGeech"
        ref = "https://github.com/captainGeech42/implant.js"
        rev = 1
    strings:
        $s1 = "ffi_callback"
        $s2 = "FetchModule"
        $s3 = "SendDebugResp"
        $s4 = "RecvDebugCmd"
        $s5 = "seek_file"
        $v8_1 = "_ZTHN2v88internal12trap_handler"
        $v8_2 = "v8::internal::v8_flags"
    condition:
        (
            (uint32be(0) == 0x7f454c46) or
            (uint16be(0) == 0x4d5a and uint16be(uint32(0x3c)) == 0x5045)
        ) and
        filesize > 10MB and
        any of ($v8_*) and 3 of ($s*)
}

rule HUNTING_implantjs_code_1 {
    meta:
        desc = "Hunt for code sequences in implant.js client implant"
        author = "captainGeech"
        ref = "https://github.com/captainGeech42/implant.js"
        rev = 1
    strings:
        $l_c1 = {488d[2]ffffff 488d7810 0fb685[2]ffff 0fb695[2]ffff 0fb68d[2]ffff 0fb6b5[2]ffff 4889bd[2]ffff 4088b5[2]ffff 80a5[2]ffff01 888d[2]ffff 80a5[2]ffff01 8895[2]ffff 80a5[2]ffff01 8885[2]ffff 80a5[2]ffff01 80bd[2]ffff00}
        $l_c2 = {e8[2]0000 488b00 4d89f0 4c89e9 4c89e2 4889de 4889c7 41ffd7 4889[3]ffff}
        $l_c3 = {488b00 4d89f9 4d89f0 4c89e9 4c89e2 4889de 4889c7 488b[3]ffff ffd0 4889[3]ffff}

        $w_c1 = {488b00 488984[3]0000 488b84[3]0000 488944[2] 488b84[3]0000 488944[2] 488b84[3]0000 488944[2] 4c8b8c[3]0000 4c8b84[3]0000 488b94[3]0000 488b8c[3]0000 ff94[3]0000 488984[3]0000}
        $w_c2 = {e8[3]ff 488bc8 e8[3]ff 488984[3]0000 488d84[3]0000 488984[3]0000 488b8c[3]0000 e8[3]ff 488984[3]0000 488b84[3]0000 488984[3]0000 488b94[3]0000 488b8c[3]0000 e8[3]ff}
    condition:
        (
            (uint32be(0) == 0x7f454c46) or
            (uint16be(0) == 0x4d5a and uint16be(uint32(0x3c)) == 0x5045)
        ) and
        filesize > 10MB and
        any of ($l_*) or any of ($w_*)
}
