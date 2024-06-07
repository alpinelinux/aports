-- from hyphen-belarusian:
	['belarusian'] = {
		loader = 'loadhyph-be.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-be.pat.txt',
		hyphenation = '',
	},
-- from hyphen-bulgarian:
	['bulgarian'] = {
		loader = 'loadhyph-bg.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-bg.pat.txt',
		hyphenation = '',
	},
-- from hyphen-churchslavonic:
	['churchslavonic'] = {
		loader = 'loadhyph-cu.tex',
		lefthyphenmin = 1,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-cu.pat.txt',
		hyphenation = 'hyph-cu.hyp.txt',
	},
-- from hyphen-mongolian:
	['mongolian'] = {
		loader = 'loadhyph-mn-cyrl.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-mn-cyrl.pat.txt',
		hyphenation = '',
	},
	['mongolianlmc'] = {
		loader = 'loadhyph-mn-cyrl-x-lmc.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		special = 'disabled:only for 8bit montex with lmc encoding',
	},
-- from hyphen-russian:
	['russian'] = {
		loader = 'loadhyph-ru.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-ru.pat.txt',
		hyphenation = 'hyph-ru.hyp.txt',
	},
-- from hyphen-serbian:
	['serbian'] = {
		loader = 'loadhyph-sr-latn.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-sh-latn.pat.txt,hyph-sh-cyrl.pat.txt',
		hyphenation = 'hyph-sh-latn.hyp.txt,hyph-sh-cyrl.hyp.txt',
	},
	['serbianc'] = {
		loader = 'loadhyph-sr-cyrl.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-sh-latn.pat.txt,hyph-sh-cyrl.pat.txt',
		hyphenation = 'hyph-sh-latn.hyp.txt,hyph-sh-cyrl.hyp.txt',
	},
-- from hyphen-ukrainian:
	['ukrainian'] = {
		loader = 'loadhyph-uk.tex',
		lefthyphenmin = 2,
		righthyphenmin = 2,
		synonyms = {  },
		patterns = 'hyph-uk.pat.txt',
		hyphenation = '',
	},
