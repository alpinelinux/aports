-- Configure Czech ispell dictionary for fulltext search.
--
-- https://postgres.cz/wiki/Instalace_PostgreSQL#Instalace_Fulltextu

CREATE TEXT SEARCH DICTIONARY cspell (template=ispell, dictfile = czech, afffile=czech, stopwords=czech);
CREATE TEXT SEARCH CONFIGURATION cs (copy=english);
ALTER TEXT SEARCH CONFIGURATION cs ALTER MAPPING FOR word, asciiword WITH cspell, simple;
