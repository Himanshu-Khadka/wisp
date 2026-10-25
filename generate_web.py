from pathlib import Path
root=Path(__file__).resolve().parent/'firmware'/'Wisp'
html=(root/'portal.html').read_text();js=(root/'portal.js').read_text()
(root/'web.h').write_text('#pragma once\nconst char WEB_PAGE[] PROGMEM=R"HTML('+html+')HTML";\nconst char PORTAL_JS[] PROGMEM=R"JS('+js+')JS";\n')
