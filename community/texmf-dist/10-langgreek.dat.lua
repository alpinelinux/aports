-- from hyphen-ancientgreek:
	['ancientgreek'] = {
		loader = 'loadhyph-grc.tex',
		lefthyphenmin = 1,
		righthyphenmin = 1,
		synonyms = {  },
		patterns = 'hyph-grc.pat.txt',
		hyphenation = '',
	},
	['ibycus'] = {
		loader = 'ibyhyph.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		special = 'disabled:8-bit only',
	},
-- from hyphen-greek:
	['greek'] = {
		loader = 'loadhyph-el-polyton.tex',
		lefthyphenmin = 1,
		righthyphenmin = 1,
		synonyms = { 'polygreek' },
		patterns = 'hyph-el-polyton.pat.txt',
		hyphenation = '',
	},
	['monogreek'] = {
		loader = 'loadhyph-el-monoton.tex',
		lefthyphenmin = 1,
		righthyphenmin = 1,
		synonyms = {  },
		patterns = 'hyph-el-monoton.pat.txt',
		hyphenation = '',
	},
