#!/usr/bin/env python3

import sys
import re

def main():

	if len(sys.argv) < 4:
		print("Usage: convert_grammar in lex_out grammar_out")
		sys.exit(1)

	in_path = sys.argv[1]
	lex_out_path = sys.argv[2]
	header_out_path = sys.argv[3]

	lines = None
	with open(in_path) as f:
		lines = f.readlines()
		# Remove newlines
		lines = map(lambda s: s.replace("\n", ""), lines)
		lines = map(lambda s: s.replace("\r", ""), lines)
		# Remove empty lines
		lines = filter(lambda s: len(s) != 0, lines)
		# Split 
		lines = list(lines)

	if len(lines) < 2:
		print("Grammar file should have at least 2 non-empty lines")
		sys.exit(1)

	# Find dividers
	if lines[0] != "/LEX/":
		print("First line should be /LEX/")
		sys.exit(1)

	try:
		grammar_divider_index = lines.index("/GRAMMAR/")
	except ValueError:
		print("Coudn't find /GRAMMAR/ divder")

	lex_lines = lines[1:grammar_divider_index]
	grammar_lines = lines[grammar_divider_index+1:]

	grammar_lines = list(filter(lambda s: not s.startswith("#"), grammar_lines))
	
	# Output lex file
	with open(lex_out_path, "w") as f:
		f.write("\n".join(lex_lines))

	# Extract lexer token names (set)
	lex_tokens = set()

	for line in lex_lines:
		# LexGetTokenInstName("VOID", "")
		match = re.search("LexGetTokenInstName\\(\\\"([A-Z_]+)\\\"", line)
		if match != None:
			lex_tokens.add(match.group(1))

	# Write include file
	with open(header_out_path, "w") as f:
		f.write("""
const vector<string> sLexicalTokenTypes = {
	"NULL",
	""")
		f.write(",\n\t".join(map(lambda s: "\"" + s + "\"", lex_tokens)))
		f.write("""
};

""")
		# TODO: Write rules
		f.write("""
const vector<RawRule> sRules = {
	""")
		def FormatRuleFromLine(line):
			columns = re.split(r"[ \t]+", line)
			# Remove empty columns
			columns = list(filter(lambda s: len(s) != 0, columns))
	
			priority = 0
			if columns[0].isnumeric():
				priority = int(columns[0])
				columns = columns[1:]

			token_name = columns[0]
			name = columns[1]
			pattern = columns[2:]
			quoted_pattern = map(lambda s: "\"" + s + "\"", pattern)
			return "RawRule(\"{tn}\", \"{n}\", {{{pat}}}, {pri})".format(tn=token_name, n=name, pat=", ".join(quoted_pattern), pri=priority)
		f.write(",\n\t".join(map(FormatRuleFromLine, grammar_lines)))

		f.write("""
};

""")


if __name__ == "__main__":
	main()

