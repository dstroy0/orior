// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Run by Ghidra's headless analyzer over a program imported with no analysis of its own: the analysis
// is run here with the analyzers the reading needs, those that find functions and disassemble their
// flow, and the rest off, as those that read DWARF, model the stack, find constant references and
// recover parameters take minutes and give nothing to a reading of instructions. Then, for each
// function named in the file the first argument gives, one a line, a line "== name" then one line a unit of the function,
// written to the file the second argument gives. The function is the one whose entry carries the name,
// as a function's name or as a label at it, so a local that link-time optimization renamed is found
// by either name. Its units are every address from its entry up to the next function's entry, as the
// disassembler reads a symbol up to the next, the gaps its flow left within the body and the bytes
// after it among them: an instruction Ghidra disassembled as "address length MNEMONIC", and every
// run of bytes Ghidra left undefined or took as data, which its flow never reached, raw as
// "address length ? hexbytes", at most BYTES_A_RUN bytes a line, so that the reader reads those bytes
// itself. A function Ghidra did not find is written as "== name" with no units.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class FunctionsDisassembled extends GhidraScript {
    private static final int BYTES_A_RUN = 16;

    private static final String[] ANALYZERS_OFF = {
        "DWARF", "Stack", "x86 Constant Reference Analyzer", "Decompiler Parameter ID", "Call Convention ID",
        "Embedded Media", "Demangler GNU", "Variadic Function Signature Override", "Apply Data Archives",
        "Scalar Operand References", "Create Address Tables", "Data Reference", "ASCII Strings",
    };

    private Function named(String name) {
        FunctionManager functions = currentProgram.getFunctionManager();
        SymbolIterator symbols = currentProgram.getSymbolTable().getSymbols(name);
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            Function function = functions.getFunctionAt(symbol.getAddress());
            if (function != null) {
                return function;
            }
        }
        return null;
    }

    private void unitsWritten(PrintWriter out, Address from, Address to) throws Exception {
        Listing listing = currentProgram.getListing();
        Address at = from;
        while (at != null && at.compareTo(to) <= 0) {
            Instruction instruction = listing.getInstructionAt(at);
            if (instruction != null) {
                out.println(at.getOffset() + " " + instruction.getLength() + " " + instruction.getMnemonicString());
                at = at.add(instruction.getLength());
                continue;
            }
            StringBuilder raw = new StringBuilder();
            int length = 0;
            Address run = at;
            while (at != null && at.compareTo(to) <= 0 && listing.getInstructionAt(at) == null && length < BYTES_A_RUN) {
                raw.append(String.format("%02x", currentProgram.getMemory().getByte(at) & 0xff));
                length += 1;
                at = at.next();
            }
            out.println(run.getOffset() + " " + length + " ? " + raw);
        }
    }

    @Override
    public void run() throws Exception {
        for (String analyzer : ANALYZERS_OFF) {
            setAnalysisOption(currentProgram, analyzer, "false");
        }
        analyzeAll(currentProgram);
        String[] arguments = getScriptArgs();
        List<String> wanted = Files.readAllLines(Paths.get(arguments[0]));
        FunctionManager functions = currentProgram.getFunctionManager();
        try (PrintWriter out = new PrintWriter(new FileWriter(arguments[1]))) {
            for (String name : wanted) {
                if (name.isEmpty()) {
                    continue;
                }
                out.println("== " + name);
                Function function = named(name);
                if (function == null) {
                    continue;
                }
                Address entry = function.getEntryPoint();
                Address last = function.getBody().getMaxAddress();
                FunctionIterator following = functions.getFunctions(entry.next(), true);
                while (following.hasNext()) {
                    Address next = following.next().getEntryPoint();
                    if (next.compareTo(entry) > 0) {
                        last = next.previous();
                        break;
                    }
                }
                unitsWritten(out, entry, last);
            }
        }
    }
}
