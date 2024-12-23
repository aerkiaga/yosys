/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Claire Xenia Wolf <claire@yosyshq.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "kernel/yosys.h"
#include "kernel/sigtools.h"
#include "kernel/ffinit.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct DffinitPass : public Pass {
	DffinitPass() : Pass("dffinit", "set INIT param on FF cells") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    dffinit [options] [selection]\n");
		log("\n");
		log("This pass sets an FF cell parameter to the the initial value of the net it\n");
		log("drives. (This is primarily used in FPGA flows.)\n");
		log("\n");
		log("    -ff <cell_name> <output_port> <init_param>\n");
		log("        operate on the specified cell type. this option can be used\n");
		log("        multiple times.\n");
		log("\n");
		log("    -highlow\n");
		log("        use the string values \"high\" and \"low\" to represent a single-bit\n");
		log("        initial value of 1 or 0. (multi-bit values are not supported in this\n");
		log("        mode.)\n");
		log("\n");
		log("    -strinit <string for high> <string for low> \n");
		log("        use string values in the command line to represent a single-bit\n");
		log("        initial value of 1 or 0. (multi-bit values are not supported in this\n");
		log("        mode.)\n");
		log("\n");
		log("    -noreinit\n");
		log("        fail if the FF cell has already a defined initial value set in other\n");
		log("        passes and the initial value of the net it drives is not equal to\n");
		log("        the already defined initial value.\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing DFFINIT pass (set INIT param on FF cells).\n");

		dict<IdString, dict<IdString, IdString>> ff_types;
		bool highlow_mode = false, noreinit = false;
		std::string high_string, low_string;

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			if (args[argidx] == "-highlow") {
				highlow_mode = true;
				high_string = "high";
				low_string = "low";
				continue;
			}
			if (args[argidx] == "-strinit" && argidx+2 < args.size()) {
				highlow_mode = true;
				high_string = args[++argidx];
				low_string = args[++argidx];
				continue;
			}
			if (args[argidx] == "-ff" && argidx+3 < args.size()) {
				IdString cell_name = RTLIL::escape_id(args[++argidx]);
				IdString output_port = RTLIL::escape_id(args[++argidx]);
				IdString init_param = RTLIL::escape_id(args[++argidx]);
				ff_types[cell_name][output_port] = init_param;
				continue;
			}
			if (args[argidx] == "-noreinit") {
				noreinit = true;
				continue;
			}
			break;
		}
		extra_args(args, argidx, design);

		for (auto module : design->selected_modules())
		{
			SigMap sigmap(module);
			FfInitVals initvals(&sigmap, module);

			for (auto cell : module->selected_cells())
			{
				if (ff_types.count(cell->type) == 0)
					continue;

				for (auto &it : ff_types[cell->type])
				{
					if (!cell->hasPort(it.first))
						continue;

					SigSpec sig = sigmap(cell->getPort(it.first));
					Const value;

					if (cell->hasParam(it.second))
						value = cell->getParam(it.second);

					Const initval = initvals(sig);
					initvals.remove_init(sig);
					for (int i = 0; i < GetSize(sig); i++) {
						if (initval[i] == State::Sx)
							continue;
						while (GetSize(value) <= i)
							value.bits().push_back(State::S0);
						if (noreinit && value[i] != State::Sx && value[i] != initval[i])
							log_error("Trying to assign a different init value for %s.%s.%s which technically "
									"have a conflicted init value.\n",
									log_id(module), log_id(cell), log_id(it.second));
						value.bits()[i] = initval[i];
					}

					if (highlow_mode && GetSize(value) != 0) {
						if (GetSize(value) != 1)
							log_error("Multi-bit init value for %s.%s.%s is incompatible with -highlow mode.\n",
									log_id(module), log_id(cell), log_id(it.second));
						if (value[0] == State::S1)
							value = Const(high_string);
						else
							value = Const(low_string);
					}

					if (value.size() != 0) {
						log("Setting %s.%s.%s (port=%s, net=%s) to %s.\n", log_id(module), log_id(cell), log_id(it.second),
								log_id(it.first), log_signal(sig), log_signal(value));
						cell->setParam(it.second, value);
					}
				}
			}
		}
	}
} DffinitPass;

PRIVATE_NAMESPACE_END
