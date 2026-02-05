import streamlit as st
import os
import google.generativeai as genai
import faiss
import numpy as np
import json
import random
import time
import base64
import re
import logging
import threading
import shutil
import urllib.parse
import requests
import datetime
from PyPDF2 import PdfReader
from dotenv import load_dotenv
from langchain_text_splitters import RecursiveCharacterTextSplitter
from langchain_community.vectorstores import FAISS
from langchain_google_genai import GoogleGenerativeAIEmbeddings
from fpdf import FPDF
import markdown

# Configure Logging
logging.basicConfig(level=logging.ERROR)

def latex_to_html(text):
    """Converts LaTeX code to basic HTML for FPDF."""
    # 1. Preamble & Comments
    if "\\begin{document}" in text: text = text.split("\\begin{document}")[1]
    if "\\end{document}" in text: text = text.split("\\end{document}")[0]
    text = re.sub(r'%.*?$', '', text, flags=re.MULTILINE)
    
    # 2. Structure Removal
    text = re.sub(r'\\tableofcontents', '', text)
    text = re.sub(r'\\listoffigures', '', text)
    text = re.sub(r'\\maketitle', '', text)
    
    # 3. Images (TikZ & Prompt Markers)
    def get_poll_url(match):
        # Extract query or use default
        try: query = match.group(1) 
        except: query = "scientific diagram schematic"
        encoded = urllib.parse.quote(query)
        # Generate HTML Image Tag
        return f'<br><img src="https://image.pollinations.ai/prompt/{encoded}?width=600&height=300&nologo=true&seed={int(datetime.datetime.now().timestamp())}" width="500"><br>'

    # Handle [IMAGE: ...] prompt markers
    text = re.sub(r'\[IMAGE: (.*?)\]', get_poll_url, text)
    # Handle TikZ diagrams
    text = re.sub(r'\\begin\{tikzpicture\}.*?\\end\{tikzpicture\}', '<br><br><b>[Abbildung (Generiert)]</b><br>' + get_poll_url(None), text, flags=re.DOTALL)
    text = re.sub(r'\\begin\{figure\}.*?\\end\{figure\}', '', text, flags=re.DOTALL)
    
    # 4. Headers (Styled)
    text = re.sub(r'\\section\*?\{(.*?)\}', r'<h1 style="color:#003366">\1</h1><br>', text)
    text = re.sub(r'\\subsection\*?\{(.*?)\}', r'<h2 style="color:#004C99">\1</h2><br>', text)
    text = re.sub(r'\\subsubsection\*?\{(.*?)\}', r'<h3>\1</h3><br>', text)
    
    # 5. Formatting (Bold, Italic)
    # Highlight important terms in Dark Red, Underline in Blue
    text = re.sub(r'\\textbf\{(.*?)\}', r'<b style="color:#990000">\1</b>', text)
    text = re.sub(r'\\textit\{(.*?)\}', r'<i>\1</i>', text)
    text = re.sub(r'\\underline\{(.*?)\}', r'<u style="color:#0033CC">\1</u>', text)
    text = re.sub(r'\\emph\{(.*?)\}', r'<i>\1</i>', text)
    
    # 6. Math Subscripts (U_b -> U_b)
    # Remove Math Mode Delimiters
    text = text.replace('$', '')
    # Convert Subscripts: X_y -> X<sub>y</sub>
    text = re.sub(r'(\w)_(\w)', r'\1<sub>\2</sub>', text)
    text = re.sub(r'(\w)_\{([^\}]+)\}', r'\1<sub>\2</sub>', text)
    # Convert Superscripts: X^y -> X<sup>y</sup>
    text = re.sub(r'(\w)\^(\w)', r'\1<sup>\2</sup>', text)
    
    # 7. Tables (Simplified)
    text = re.sub(r'\\begin\{tabular\}.*?', '', text)
    text = re.sub(r'\\begin\{longtable\}.*?', '', text)
    text = text.replace('\\end{tabular}', '')
    text = text.replace('\\end{longtable}', '')
    text = text.replace('\\hline', '')
    text = text.replace('&', ' | ')
    text = text.replace('\\\\', '<br>')
    
    # 8. Lists
    text = text.replace('\\begin{itemize}', '<ul>')
    text = text.replace('\\end{itemize}', '</ul>')
    text = text.replace('\\begin{enumerate}', '<ol>')
    text = text.replace('\\end{enumerate}', '</ol>')
    text = re.sub(r'\\item\s*', '<li>', text)
    text = text.replace('</li><li>', '</li><li>') # Simple fix implies manual closure not strictly needed in simple HTMLMixin sometimes but better safe:
    # FPDF HTMLMixin is tolerant.
    
    # 9. Cleanup
    text = re.sub(r'\\[a-zA-Z]+\{.*?\}', '', text) # Remove generic commands
    text = re.sub(r'\\[a-zA-Z]+', '', text) # Remove verbs
    text = text.replace('{', '').replace('}', '') # Remove braces
    
    # Newlines to BR
    text = re.sub(r'\n\s*\n', '<br><br>', text)
    text = text.replace('\n', ' ') # Inline newlines to space
    
    return text

def create_fallback_pdf(text, source_pdf_stream=None):
    """Generates a rich PDF locally using HTML conversion and extracting source images."""
    try:
        pdf = FPDF()
        pdf.add_page()
        pdf.set_auto_page_break(auto=True, margin=15)
        pdf.set_font("Helvetica", size=11)
        
        # HTML Title
        title_html = """
        <h1 align="center" style="color:#003366; font-size:24pt;">Zusammenfassung</h1>
        <h4 align="center" style="color:#666666;">(Rich Text & Originalgrafiken)</h4>
        <br><hr><br>
        """
        pdf.write_html(title_html)
        
        # Convert Body
        html_body = latex_to_html(text)
        
        # Unicode Safety (Basic Latin-1 transliteration)
        unicode_map = {
            'ä': 'ae', 'ö': 'oe', 'ü': 'ue', 'Ä': 'Ae', 'Ö': 'Oe', 'Ü': 'Ue', 'ß': 'ss',
            '“': '"', '”': '"', '–': '-', '…': '...'
        }
        for k, v in unicode_map.items():
            html_body = html_body.replace(k, v)
            
        pdf.write_html(html_body)
        
        # APPENDIX: Original Images from Source PDF
        if source_pdf_stream:
            try:
                import io
                if hasattr(source_pdf_stream, 'seek'): source_pdf_stream.seek(0)
                
                reader = PdfReader(source_pdf_stream)
                images_found = False
                
                count = 0
                for page in reader.pages:
                    if count > 20: break
                    if hasattr(page, 'images') and page.images:
                        for img in page.images:
                            if count > 20: break
                            
                            if not images_found:
                                pdf.add_page()
                                pdf.set_font("Helvetica", 'B', 16)
                                pdf.set_text_color(0, 51, 102)
                                pdf.cell(0, 10, "Anhang: Grafiken aus Original-PDF", ln=True)
                                pdf.ln(5)
                                images_found = True
                            
                            try:
                                pdf.image(io.BytesIO(img.data), w=160)
                                pdf.ln(5)
                                count += 1
                            except: pass
            except Exception as e_img:
                print(f"Extraction Error: {e_img}")
        
        return bytes(pdf.output())
    except Exception as e:
        print(f"Fallback PDF Error: {e}")
        return None
load_dotenv()

# Page Config
st.set_page_config(page_title="Blop Study", layout="wide")
st.title("Blop Study - Smart AI Learning Companion")

# --- Helper Functions ---

def create_markdown_pdf(md_text, source_pdf_stream=None):
    """Converts Markdown to PDF using FPDF."""
    try:
        # Convert MD -> HTML
        html_content = markdown.markdown(md_text)
        
        pdf = FPDF()
        pdf.add_page()
        pdf.set_auto_page_break(auto=True, margin=15)
        pdf.set_font("Helvetica", size=11)
        
        # Header
        pdf.write_html('<h1 align="center" style="color:#003366">Zusammenfassung</h1><br><hr><br>')
        
        # Robust Unicode Handling for FPDF (Helvetica)
        # 1. Manual Map for common German chars
        unicode_map = {
            'ä': 'ae', 'ö': 'oe', 'ü': 'ue', 'Ä': 'Ae', 'Ö': 'Oe', 'Ü': 'Ue', 'ß': 'ss',
            '“': '"', '”': '"', '–': '-', '…': '...'
        }
        for k, v in unicode_map.items():
            html_content = html_content.replace(k, v)
        
        # 2. Remove internal links (FPDF crash fix: "Named destination...")
        html_content = re.sub(r'<a href="#.*?">(.*?)</a>', r'\1', html_content)

        # 3. Bruteforce sanitize remainder (critical for FPDF stability)
        html_content = html_content.encode('latin-1', 'replace').decode('latin-1')
            
        pdf.write_html(html_content)
        
        # APPENDIX
        if source_pdf_stream:
            try:
                import io
                if hasattr(source_pdf_stream, 'seek'): source_pdf_stream.seek(0)
                reader = PdfReader(source_pdf_stream)
                images_found = False
                count = 0
                for page in reader.pages:
                    if count > 20: break
                    if hasattr(page, 'images') and page.images:
                        for img in page.images:
                            if count > 20: break
                            if not images_found:
                                pdf.add_page()
                                pdf.set_font("Helvetica", 'B', 16)
                                pdf.set_text_color(0, 51, 102)
                                pdf.cell(0, 10, "Anhang: Grafiken aus Original-PDF", ln=True)
                                pdf.ln(5)
                                images_found = True
                            try:
                                pdf.image(io.BytesIO(img.data), w=160)
                                pdf.ln(5)
                                count += 1
                            except: pass
            except: pass
            
        return bytes(pdf.output())
    except Exception as e:
        print(f"MD PDF Error: {e}")
        st.error(f"Fehler bei PDF-Erstellung (Fallback): {e}")
        return None

def get_generative_model_name():
    """Dynamically finds a working model name."""
    try:
        all_models = [m for m in genai.list_models() if 'generateContent' in m.supported_generation_methods]
        
        # Priority 1: High Quota Stable Model (1.5 Flash)
        for m in all_models:
            if "gemini-1.5-flash" in m.name and "001" in m.name: # e.g. gemini-1.5-flash-001
                return m.name
        for m in all_models:
            if "gemini-1.5-flash" in m.name:
                return m.name
                
        # Priority 2: Pro (Better quality, lower quota, but stable)
        for m in all_models:
            if "gemini-1.5-pro" in m.name: return m.name
        
        # Priority 3: Fallback to any flash (e.g. 2.5)
        for m in all_models:
            if "flash" in m.name: return m.name
            
        return all_models[0].name
    except:
        pass
    
    # Fallback default if list fails
    return "gemini-1.5-flash"

def get_embedding_model_name():
    """Dynamically finds a working embedding model name."""
    try:
        for m in genai.list_models():
            if 'embedContent' in m.supported_generation_methods:
                return m.name
    except:
        pass
    return "models/embedding-001"

@st.cache_data(show_spinner=False)
def get_pdf_text(pdf_docs):
    pages_data = []
    for pdf in pdf_docs:
        pdf_reader = PdfReader(pdf)
        for i, page in enumerate(pdf_reader.pages):
            t = page.extract_text()
            if t:
                # Store metadata
                pages_data.append({
                    "page": i + 1,
                    "text": t,
                    "source": pdf.name
                })
    return pages_data

def get_text_chunks(pages_data, chunk_size=1000, chunk_overlap=200):
    chunks = []
    for p in pages_data:
        text = p['text']
        page_num = p['page']
        source = p['source']
        
        start = 0
        while start < len(text):
            end = start + chunk_size
            chunk_text = text[start:end]
            # Chunk is now a Dict
            chunks.append({
                "text": chunk_text,
                "page": page_num,
                "source": source
            })
            start = end - chunk_overlap
    return chunks

def build_vector_store(text_chunks):
    # ... (Keep existing implementation)
    embeddings = []
    valid_chunks = []
    
    model = get_embedding_model_name()
    
    # Safe Mode: Process one chunk at a time to minimize complexity
    batch_size = 1 
    progress_bar = st.progress(0)
    total_chunks = len(text_chunks)
    
    for i in range(0, total_chunks, batch_size):
        batch = text_chunks[i:i + batch_size]
        # Extract plain text for embedding
        batch_text = [c['text'] for c in batch]
        
        retries = 0
        while retries < 5:
            try:
                # Batch embedding request
                result = genai.embed_content(
                    model=model,
                    content=batch_text,
                    task_type="retrieval_document",
                    title="Study Material"
                )
                
                # result['embedding'] is a list of embeddings when input is a list
                batch_embeddings = result['embedding']
                
                embeddings.extend(batch_embeddings)
                valid_chunks.extend(batch)
                
                # Success!
                # time.sleep(1) # Removed artificial delay for performance
                break
            except Exception as e:
                if "429" in str(e):
                    # Quote Exceeded? Wait significantly longer.
                    wait_time = 20 * (retries + 1) # 20s, 40s, 60s...
                    st.warning(f"Rate Limit (429) bei Index {i}. Warte {wait_time} Sekunden...")
                    time.sleep(wait_time)
                    retries += 1
                    if retries == 5:
                         st.error(f"Konnte nach {retries} Versuchen nicht fortfahren. Die API-Quota ist wahrscheinlich erschöpft. Bitte später versuchen.")
                else:
                    st.warning(f"Fehler bei Index {i}: {e}")
                    break
                    
        progress_bar.progress(min((i + batch_size) / total_chunks, 1.0))
        
    if not embeddings:
        return None, []
        
    embeddings_np = np.array(embeddings, dtype='float32')
    dimension = embeddings_np.shape[1]
    index = faiss.IndexFlatL2(dimension)
    index.add(embeddings_np)
    
    return index, valid_chunks

def answer_question(query, index, chunks):
    try:
        # Embed query
        embedding_model = get_embedding_model_name()
        result = genai.embed_content(
            model=embedding_model,
            content=query,
            task_type="retrieval_query"
        )
        q_emb = np.array([result['embedding']], dtype='float32')
        
        # Search
        D, I = index.search(q_emb, k=3)
        
        # Retrieve Context with Page Numbers
        context_chunks = [chunks[i] for i in I[0] if i < len(chunks)]
        context_text = ""
        for c in context_chunks:
            source_file = os.path.basename(c.get('source', 'Doc'))
            page_num = c.get('page', '?')
            context_text += f"\nSOURCE: {source_file} (Seite {page_num}):\n{c['text']}\n---"
        
        # Generate Answer
        model_name = get_generative_model_name()
        # Clean model name if necessary (remove 'models/' if sdk adds it automatically? No, sdk handles it)
        model = genai.GenerativeModel(model_name)
        
        prompt = f"""
        Beantworte die folgende Frage basierend auf dem bereitgestellten Kontext.
        Wenn die Antwort nicht im Kontext steht, sage das.
        
        WICHTIG: Gib die Seitenzahl an, wenn du Informationen aus dem Kontext verwendest (z.B. [Seite 5]).
        
        Frage: {query}
        
        Kontext:
        {context_text}
        """
        
        response = model.generate_content(prompt)
        return response.text, context_text
    except Exception as e:
        return f"Fehler bei der Antwortgenerierung: {str(e)}", ""

def generate_quiz(chunks):
    if not chunks:
        return None
        
    try:
        # Pick random chunks
        selected = random.sample(chunks, min(3, len(chunks)))
        context = ""
        for c in selected:
            context += f"\n[Seite {c.get('page','?')}]: {c['text']}\n"
        
        model_name = get_generative_model_name()
        model = genai.GenerativeModel(model_name)
        
        prompt = f"""
        Basierend auf folgendem Text, erstelle 3 Multiple-Choice-Fragen.
        Der Output MUSS valides JSON sein in diesem Format:
        [
            {{
                "question": "Frage hier (inkl. Seitenreferenz falls möglich)",
                "options": ["Option A", "Option B", "Option C", "Option D"],
                "answer": "Die richtige Option (exakter String)"
            }}
        ]
        
        Text:
        {context}
        """
        
        response = model.generate_content(prompt)
        
        # Parse JSON
        content = response.text
        if "```json" in content:
            content = content.split("```json")[1].split("```")[0]
        elif "```" in content:
            content = content.split("```")[1].split("```")[0]
        return json.loads(content)
    except Exception as e:
        st.error(f"Fehler bei Quiz-Generierung: {e}")
        return None

def generate_ics(plan_data, start_date):
    """Generates a basic iCalendar string from plan data."""
    cal_content = [
        "BEGIN:VCALENDAR",
        "VERSION:2.0",
        "PRODID:-//StudyFlow//Study Plan//EN",
        "CALSCALE:GREGORIAN",
        "METHOD:PUBLISH"
    ]
    
    # current_date = start_date # Not needed if we parse dates
    now_str = datetime.datetime.now().strftime("%Y%m%dT%H%M%SZ")
    
    for i, day_item in enumerate(plan_data):
        date_str_raw = day_item.get('date', '')
        event_date = None
        
        # Try to parse strict date format
        try:
            event_date = datetime.datetime.strptime(date_str_raw, "%d.%m.%Y").date()
        except:
            # Fallback: Use start_date + index
            event_date = start_date + datetime.timedelta(days=i)
            
        date_str = event_date.strftime("%Y%m%d")
        
        # Clean text
        summary = f"Lernen: {day_item.get('topic', 'Thema')}"
        desc = day_item.get('details', '').replace('\n', '\\n')
        
        # Unique ID
        uid = f"{date_str}-{i}@blop-study.app"
        
        cal_content.extend([
            "BEGIN:VEVENT",
            f"DTSTART;VALUE=DATE:{date_str}",
            f"DTSTAMP:{now_str}",
            f"UID:{uid}",
            f"SUMMARY:{summary}",
            f"DESCRIPTION:{desc}",
            "STATUS:CONFIRMED",
            "TRANSP:TRANSPARENT",
            "END:VEVENT"
        ])
        
    cal_content.append("END:VCALENDAR")
    return "\n".join(cal_content)

def get_summary_prompt(mode, text, language="Deutsch"):
    if "LaTeX" in mode:
        return f"""
**Rolle & Ziel**
Du bist ein professioneller Typesetter (Setzer).
**AUFGABE**: Konvertiere den folgenden **Markdown-Inhalt** (Input) exakt in das bereitgestellte **LaTeX-Template**.

**WICHTIG:**
1. **Inhalt**: Übernimm Text, Struktur, Überschriften und Bulletpoints **1:1**. **Schreibe KEINE neue Zusammenfassung**, sondern formatiere den existierenden Text!
2. **Tabellen**: Konvertiere Markdown-Tabellen in professionelle LaTeX-Tabellen (`booktabs`).
3. **Bilder**: Wenn du `[IMAGE: ...]` findest, ersetze es durch:
   `\\begin{{center}}\\textcolor{{accent}}{{\\textit{{[Hier Grafik einfügen: ...]}}}}\\end{{center}}` (oder TikZ, falls passend).

**Design-Elemente:**
1. **Highlight-Boxen**: Nutze `\\begin{{merkbox}}{{...}}...\\end{{merkbox}}` für Definitionen/Wichtiges.
2. **Farben**: `primary` (Blau) für Überschriften.
3. **Interaktivität**: Folgt aus `hyperref`.

**LaTeX-Template:**
\\documentclass[11pt, a4paper]{{article}}
\\usepackage[utf8]{{inputenc}}
\\usepackage[ngerman]{{babel}}
\\usepackage[T1]{{fontenc}}
\\usepackage{{helvet}}
\\renewcommand{{\\familydefault}}{{\\sfdefault}}
\\usepackage[left=2.5cm,right=2.5cm,top=2.5cm,bottom=2.5cm]{{geometry}}
\\usepackage{{amsmath}}
\\usepackage{{parskip}}
\\usepackage{{xcolor}}
\\usepackage{{tikz}}
\\usetikzlibrary{{arrows.meta, positioning, calc, shadows}}
\\usepackage{{tcolorbox}}
\\usepackage{{fancyhdr}}
\\usepackage{{titlesec}}
\\usepackage[hidelinks]{{hyperref}} % Interactive TOC & Links

% --- Design Definitions ---
\\definecolor{{primary}}{{HTML}}{{005BAA}} % Modern Blue
\\definecolor{{accent}}{{HTML}}{{E63946}}  % Red Accent

% Box Style
\\tcbuselibrary{{skins}}
\\newtcolorbox{{merkbox}}[1]{{
  colback=primary!5!white,
  colframe=primary,
  title={{#1}},
  fonttitle=\\bfseries\\sffamily,
  boxrule=0.5mm,
  arc=2mm,
  drop shadow
}}

% Header/Footer
\\pagestyle{{fancy}}
\\fancyhf{{}}
\\lhead{{\\textcolor{{primary}}{{\\textbf{{Blop Study}}}}}}
\\rhead{{\\today}}
\\cfoot{{\\thepage}}
\\renewcommand{{\\headrulewidth}}{{0.4pt}}
\\renewcommand{{\\headrule}}{{\\hbox to\\headwidth{{\\color{{primary}}\\leaders\\hrule height \\headrulewidth\\hfill}}}}

% Section Styling
\\titleformat{{\\section}}
  {{\\color{{primary}}\\Large\\bfseries\\uppercase}}
  {{\\thesection}}{{1em}}{{}}

\\begin{{document}}

% --- Modern Title ---
\\thispagestyle{{empty}}
\\begin{{tikzpicture}}[remember picture, overlay]
    \\node[fill=primary, minimum width=\\paperwidth, minimum height=3cm, anchor=north] at (current page.north) {{}};
    \\node[anchor=north, text=white, yshift=-1cm] at (current page.north) {{\\fontsize{{30}}{{36}}\\selectfont \\textbf{{Zusammenfassung}}}};
\\end{{tikzpicture}}
\\vspace{{3cm}}

\\begin{{center}}
    {{\\Huge \\textbf{{[TITEL]}}}} \\\\[0.5em]
    {{\\Large \\textcolor{{gray}}{{Skript & Dokumentation}}}} \\\\[2em]
    \\today
\\end{{center}}
\\vspace{{1cm}}

\\tableofcontents
\\newpage

% --- Content ---
% KI: Hier den Markdown-Input in LaTeX konvertieren.

\\end{{document}}

**Input Markdown Content:**
{text[:50000]}

**Constraints:**
- Return ONLY valid LaTeX code. No Markdown blocks.
"""
    else:
        return f"""
**Rolle**: Du bist ein Lern-Assistent für Studenten.
**Sprache**: {language} (Antworte zwingend in dieser Sprache!).

**Aufgabe**:
Erstelle eine ausführliche, gut strukturierte Zusammenfassung des Textes.
Das Ziel ist eine **Web-Optimierte Ansicht** mit einfacher Navigation.

**Struktur (Markdown)**:
1. **Inhaltsverzeichnis (WICHTIG)**:
   - Erstelle eine Liste aller Kapitel.
   - Nutze Markdown-Anker für Interaktivität, die genau den Überschriften entsprechen.
   - Beispiel: `- [1. Einleitung](#1-einleitung)` für `## 1. Einleitung`.
   - Achte darauf, dass die Links funktionieren (Kleinschreibung, Bindestriche statt Leerzeichen).

2. **Hauptinhalt**:
   - Nutze Überschriften (## 1. ..., ### 1.1 ...).
   - Nutze **Fett** für Definitionen.
   - Nutze Tabellen wo sinnvoll.

3. **Visualisierung**:
   - Füge nach jedem Hauptkapitel ein Bild ein.
   - Format: `[IMAGE: visual description in english]`
   - Halte die Beschreibung kurz und prägnant (keine Sonderzeichen).

**Input Text**:
{text}
"""

import urllib.parse

def render_visual_summary(text):
    """Replaces [IMAGE: query] with Pollinations.ai image URLs."""
    if not text: return ""
    def replacer(match):
        query = match.group(1).strip()
        # Clean query: Remove special chars that might break URL
        # Allowing alphanumeric, commas, spaces
        clean_query = re.sub(r'[^a-zA-Z0-9,\s]', '', query)
        encoded = urllib.parse.quote(clean_query)
        # Random seed to prevent caching issues
        seed = int(datetime.datetime.now().timestamp()) + random.randint(0, 1000)
        return f"![Bild: {clean_query}](https://image.pollinations.ai/prompt/{encoded}?width=1024&height=512&nologo=true&seed={seed})"
    
    return re.sub(r'\[IMAGE: (.*?)\]', replacer, text)

def get_overleaf_url(latex_code):
    """Generates a direct Overleaf opening link."""
    try:
        # Encode as Base64 Data URI
        b64 = base64.b64encode(latex_code.encode('utf-8')).decode('utf-8')
        return f"https://www.overleaf.com/docs?snip_uri=data:application/x-tex;base64,{b64}"
    except:
        return None

def compile_latex_to_pdf(latex_code, fallback_markdown=None):
    """Compiles LaTeX to PDF using multiple Cloud APIs (Fallback Strategy)."""
    headers = {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
    }
    
    # Strategy 1: LatexOnline (HTTP)
    try:
        url = "http://latexonline.cc/compile"
        files = {'file': ('main.tex', latex_code)}
        response = requests.post(url, files=files, headers=headers, timeout=30)
        if response.status_code == 200: return response.content
    except: pass 
        
    # Strategy 2: RTeX (HTTP)
    try:
        url_api = "http://rtex.probablyaweb.site/api/v2"
        response = requests.post(url_api, data={'code': latex_code, 'format': 'pdf'}, headers=headers, timeout=30)
        if response.status_code == 200:
            data = response.json()
            if data.get('status') == 'success':
                pdf_url = f"{url_api}/{data['filename']}"
                pdf_resp = requests.get(pdf_url, headers=headers, timeout=60)
                if pdf_resp.status_code == 200: return pdf_resp.content
    except: pass
        
    st.warning("⚠️ Cloud-Server nicht erreichbar (Firewall). Generiere lokale Notfall-PDF.")
    
    # Try to get source PDF for images (Appendix)
    source_pdf = None
    if "uploaded_file" in st.session_state and st.session_state.uploaded_file:
        try:
            source_pdf = st.session_state.uploaded_file
        except: pass
        
    if fallback_markdown:
        return create_markdown_pdf(fallback_markdown, source_pdf)
    
    return None

def render_page_buttons(text):
    """Finds 'Seite X' in text and renders buttons to jump there."""
    if not text: return
    
    # Regex to find "Seite 5", "Seite 12", "[Seite 5]"
    matches = re.findall(r"Seite\s+(\d+)", text)
    unique_pages = sorted(list(set(matches)), key=lambda x: int(x))
    
    if unique_pages:
        st.write("🔎 **Fundstellen:**")
        # Use more columns for smaller buttons
        cols = st.columns(min(len(unique_pages), 10)) 
        for i, p in enumerate(unique_pages):
            if i < 20: # Limit total buttons
                col_idx = i % 10
                # Minimal button label "[5]"
                if cols[col_idx].button(f"[{p}]", key=f"jump_{p}_{hash(text)}", help=f"Gehe zu Seite {p}"):
                    st.session_state.current_pdf_page = int(p)
                    st.toast(f"Seite {p}")

def display_pdf(uploaded_file):
    """Generates HTML iframe for PDF display."""
    try:
        if hasattr(uploaded_file, 'seek'):
            uploaded_file.seek(0)
        bytes_data = uploaded_file.getvalue()
        base64_pdf = base64.b64encode(bytes_data).decode('utf-8')
        
        # Check for requested page
        page_num = st.session_state.get('current_pdf_page', 1)
        offset = st.session_state.get('page_offset', 0)
        
        real_page = max(1, page_num + offset)
        
        # Append timestamp to force reload (anti-cache)
        t = time.time()
        
        # Append #page=X
        pdf_display = f'<iframe src="data:application/pdf;base64,{base64_pdf}#page={real_page}&zoom=100&t={t}" width="100%" height="800px" type="application/pdf"></iframe>'
        return pdf_display
    except Exception as e:
        return f"Fehler bei PDF-Anzeige: {e}"

def display_pdf_bytes(pdf_bytes):
    """Generates HTML iframe from specific PDF bytes."""
    try:
        base64_pdf = base64.b64encode(pdf_bytes).decode('utf-8')
        t = time.time()
        pdf_display = f'<iframe src="data:application/pdf;base64,{base64_pdf}#view=FitH&t={t}" width="100%" height="800px" type="application/pdf"></iframe>'
        return pdf_display
    except Exception as e:
        return f"Fehler: {e}"

# ... (Sidebar code unchanged) ...

# ... (Tab 1 Code Update) ...
# I will supply the full replacement here to ensure model usage is consistent
# ... inside the button logic ...
                # model = genai.GenerativeModel(get_generative_model_name()) 


# --- Sidebar ---
with st.sidebar:
    st.header("Einstellungen")
    api_key_input = st.text_input("Gemini API Key", type="password", key="api_key_input")
    if api_key_input:
        os.environ["GOOGLE_API_KEY"] = api_key_input
        genai.configure(api_key=api_key_input)
        st.success("API Key gespeichert.")
    elif os.getenv("GOOGLE_API_KEY"):
        genai.configure(api_key=os.getenv("GOOGLE_API_KEY"))
        st.success("API Key aus Umgebung.")
    
    # Model Selector
    available_models = ["Automatisch"]
    try:
        if os.environ.get("GOOGLE_API_KEY"):
            for m in genai.list_models():
                if "generateContent" in m.supported_generation_methods:
                     # Clean name 'models/gemini-pro' -> 'gemini-pro'
                     name = m.name.replace("models/", "")
                     if name not in available_models:
                        available_models.append(name)
    except Exception as e:
        # Fallback and show error if list fails (e.g. invalid key)
        available_models.extend(["gemini-1.5-flash", "gemini-1.5-pro"])
        
    model_option = st.selectbox("AI-Modell", available_models)
        
    st.header("Dokumente")
    uploaded_files = st.file_uploader("PDFs hochladen", type=["pdf"], accept_multiple_files=True)
    show_pdf_split = st.checkbox("PDF Split-View (Groß)", value=False)
    
    # Offset setting for correct page sync
    page_offset = st.number_input("Seitenzahl-Offset", value=0, step=1, help="Korrektur, falls PDF-Seiten nicht mit Buch-Seiten übereinstimmen.")
    if "page_offset" not in st.session_state:
        st.session_state.page_offset = 0
    st.session_state.page_offset = page_offset
    
    process_button = st.button("Dokumente analysieren")
    
    file_map = {}
    if uploaded_files:
        file_map = {f.name: f for f in uploaded_files}
        
    if uploaded_files and not show_pdf_split:
        st.markdown("---")
        st.subheader("📄 PDF Vorschau")
        selected_pdf_name = st.selectbox("Anzeigen:", list(file_map.keys()))
        
        if selected_pdf_name:
            pdf_html = display_pdf(file_map[selected_pdf_name])
            st.markdown(pdf_html, unsafe_allow_html=True)

    if st.button("Debug: Modelle prüfen"):
        try:
            st.write("Verfügbare Modelle:")
            for m in genai.list_models():
                if "generateContent" in m.supported_generation_methods:
                    st.code(m.name)
        except Exception as e:
            st.error(f"Fehler: {e}")

if process_button and uploaded_files:
    if not os.environ.get("GOOGLE_API_KEY"):
         st.error("API Key fehlt!")
    else:
        with st.spinner("Extrahiere Text..."):
            raw_text = get_pdf_text(uploaded_files)
        
        with st.spinner("Erstelle Embeddings (Das kann dauern)..."):
            chunks = get_text_chunks(raw_text)
            index, valid_chunks = build_vector_store(chunks)
            
            if index:
                st.session_state.vector_index = index
                st.session_state.text_chunks = valid_chunks
                st.session_state.total_pages = sum([len(PdfReader(f).pages) for f in uploaded_files])
                st.success(f"Fertig! {len(valid_chunks)} Textblöcke indexiert.")
            else:
                st.error("Fehler beim Erstellen des Index.")

# --- Layout Logic ---
# --- Layout Logic ---
main_container = st.container()
if show_pdf_split and uploaded_files:
    # Sticky CSS for Right Column
    st.markdown("""
        <style>
        div[data-testid="column"]:nth-of-type(2) {
            position: sticky;
            top: 1rem;
            height: 98vh;
            overflow: hidden; 
        }
        </style>
        """, unsafe_allow_html=True)

    c1, c2 = st.columns([1, 1])
    main_container = c1
    
    with c2:
        st.subheader("📄 PDF Ansicht")
        if file_map:
            # Sync selection with sidebar? use distinct key
            sel_pdf = st.selectbox("Dokument:", list(file_map.keys()), key="pdf_sel_split")
            st.markdown(display_pdf(file_map[sel_pdf]), unsafe_allow_html=True)

with main_container:
    tab1, tab2, tab3, tab4 = st.tabs(["📅 Lernplan", "💬 Dozenten-Chat", "📝 Quiz", "📑 Zusammenfassung"])

import datetime

with tab1:
    st.header("Smarter Lernplan")
    
    col1, col2 = st.columns(2)
    with col1:
        start_date = st.date_input("Start-Datum", min_value=datetime.date.today(), value=datetime.date.today())
    with col2:
        exam_date = st.date_input("Prüfungs-Datum", min_value=datetime.date.today() + datetime.timedelta(days=1), value=datetime.date.today() + datetime.timedelta(days=7))
        
    focus_topic = st.text_input("Fokus-Thema (optional)", placeholder="z.B. Differentialgleichungen")
    
    # Frequency Selector
    st.subheader("Lern-Intervall")
    available_days = ["Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag"]
    selected_days = st.multiselect("An welchen Tagen möchtest du lernen?", available_days, default=["Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag"])
    
    # Calculate study dates
    study_dates = []
    current = start_date
    while current <= exam_date:
        # python weekday: 0=Mon, 6=Sun
        day_name = available_days[current.weekday()]
        if day_name in selected_days:
            study_dates.append(current)
        current += datetime.timedelta(days=1)
    
    days_count = len(study_dates)
    
    if st.button("Lernplan erstellen (AI)"):
        if "text_chunks" not in st.session_state:
            st.warning("Bitte erst Dokumente hochladen!")
        elif days_count == 0:
            st.error("Bitte mindestens einen Wochentag auswählen!")
        else:
            st.info(f"Erstelle Plan für {days_count} Lern-Sessions zwischen {start_date} und {exam_date}...")
            
            chunks = st.session_state.text_chunks
            sample_chunks = chunks[:2] + random.sample(chunks[2:], min(3, len(chunks)-2)) if len(chunks) > 2 else chunks
            
            # Format context with page numbers for the plan
            context_text = ""
            for c in sample_chunks:
                context_text += f"\n[Seite {c.get('page', '?')}] {c.get('text', '')[:1500]}\n"
            
            date_list_str = ", ".join([d.strftime('%d.%m.%Y') for d in study_dates])
            
            prompt = f"""
            Erstelle einen Lernplan für genau {days_count} Lern-Einheiten an diesen Daten: {date_list_str}.
            Fokus: {focus_topic if focus_topic else "Allgemein"}.
            Inhalt: {context_text}
            
            WICHTIG: Nutze Markdown für die Details!
            - Verwende **Fett** für wichtige Begriffe und Überschriften.
            - Nutze Listen (Bullet points) für bessere Lesbarkeit.
            - Markiere Definitionen oder Schlüsselsätze.
            
            Output MUSS valides JSON sein:
            [
                {{ "date": "DD.MM.YYYY", "topic": "Titel", "details": "**Ziele**:\\n- Punkt 1\\n- Punkt 2..." }},
                ...
            ]
            """
            
            try:
                # Determine model
                if model_option == "Automatisch":
                    model_name = get_generative_model_name()
                else:
                    model_name = model_option
                
                st.toast(f"Verwende Modell: {model_name}")
                model = genai.GenerativeModel(model_name)
                with st.spinner(f"AI ({model_name}) generiert deinen persönlichen Plan..."):
                    response = model.generate_content(prompt)
                    
                    content = response.text
                    if "```json" in content:
                        content = content.split("```json")[1].split("```")[0]
                    elif "```" in content:
                        content = content.split("```")[1].split("```")[0]
                    
                    data = json.loads(content)
                    st.session_state.plan_data = data
                    st.rerun()
                
            except Exception as e:
                st.error(f"Fehler bei der Plan-Erstellung: {e}")

    # --- Render Interactive Plan ---
    if "plan_data" in st.session_state:
        st.subheader(f"Dein Fahrplan ({len(st.session_state.plan_data)} Einheiten)")
        
        # Actions Row
        ac1, ac2 = st.columns([1, 4])
        with ac1:
            if st.button("➕ Tag hinzufügen"):
                # Add next day? Or just generic
                next_date = datetime.date.today().strftime('%d.%m.%Y')
                st.session_state.plan_data.append({"date": next_date, "topic": "Neues Thema", "details": "..."})
                st.rerun()
        
        # Export
        if len(st.session_state.plan_data) > 0:
            ics_data = generate_ics(st.session_state.plan_data, start_date)
            with ac2:
                st.download_button(
                    label="📅 Export für Google Kalender (.ics)",
                    data=ics_data,
                    file_name="lernplan.ics",
                    mime="text/calendar"
                )
        
        # Plan Items
        for i, item in enumerate(st.session_state.plan_data):
            date_label = item.get('date', f'Tag {i+1}')
            topic_label = item.get('topic', 'Thema')
            
            with st.expander(f"📅 {date_label}: {topic_label}", expanded=False):
                # Edit Mode vs Preview
                tab_edit, tab_view = st.tabs(["Bearbeiten", "Vorschau"])
                
                with tab_edit:
                    col_a, col_b = st.columns([5, 1])
                    with col_a:
                        new_date = st.text_input("Datum", item.get('date', ''), key=f"date_{i}")
                        new_topic = st.text_input("Thema", item.get('topic', ''), key=f"topic_{i}")
                        # Taller text area for better editing experience
                        new_details = st.text_area("Details (Markdown)", item.get('details', ''), height=450, key=f"desc_{i}")
                        
                        st.session_state.plan_data[i]['date'] = new_date
                        st.session_state.plan_data[i]['topic'] = new_topic
                        st.session_state.plan_data[i]['details'] = new_details
                    with col_b:
                        st.write("")
                        if st.button("🗑️", key=f"del_{i}"):
                            st.session_state.plan_data.pop(i)
                            st.rerun()
                
                with tab_view:
                    st.markdown(f"### {item.get('topic', '')}")
                    st.markdown(item.get('details', ''))
                    
                    st.markdown("---")
                    render_page_buttons(item.get('details', ''))

with tab2:
    st.header("Dozenten-Modus")
    
    if "messages" not in st.session_state:
        st.session_state.messages = []
        
    for msg in st.session_state.messages:
        with st.chat_message(msg["role"]):
            st.markdown(msg["content"])
            
    if prompt := st.chat_input("Frage stellen..."):
        if "vector_index" not in st.session_state:
            st.error("Bitte Dokumente hochladen.")
        else:
            st.session_state.messages.append({"role": "user", "content": prompt})
            with st.chat_message("user"):
                st.markdown(prompt)
            
            with st.spinner("Suche Antwort..."):
                answer, context = answer_question(prompt, st.session_state.vector_index, st.session_state.text_chunks)
                
                full_resp = f"{answer}" # Context is optional to show
                st.session_state.messages.append({"role": "assistant", "content": full_resp})
                with st.chat_message("assistant"):
                    st.markdown(full_resp)

with tab3:
    st.header("Quiz-Modus")
    if st.button("Quiz erstellen"):
        if "text_chunks" not in st.session_state:
             st.warning("Dokumente fehlen.")
        else:
            with st.spinner("Generiere Quiz..."):
                quiz = generate_quiz(st.session_state.text_chunks)
                if quiz:
                    st.session_state.quiz_data = quiz
                    st.session_state.user_answers = {}
                    st.rerun()
                else:
                    st.error("Fehler bei Quiz-Generierung.")
                    
    if "quiz_data" in st.session_state:
        for i, q in enumerate(st.session_state.quiz_data):
            st.subheader(f"Frage {i+1}: {q['question']}")
            choice = st.radio("Antwort:", q['options'], key=f"q_{i}")
            if st.button(f"Prüfen {i+1}", key=f"btn_{i}"):
                if choice == q['answer']:
                    st.success("Korrekt!")
                else:
                    st.error(f"Falsch. Richtig: {q['answer']}")

with tab4:
    st.header("Zusammenfassung & Notizen")
    
    st.info("Erstelle eine interaktive Web-Zusammenfassung mit KI-Bildern. Das offizielle PDF (Skript-Stil) kannst du anschließend herunterladen.")
    
    # Language Selection
    summary_lang = st.selectbox("Sprache der Zusammenfassung:", ["Deutsch", "English", "Français", "Español"], index=0)
    
    if st.button("Zusammenfassung erstellen (AI)"):
        if "text_chunks" not in st.session_state:
            st.warning("Bitte erst Dokumente hochladen!")
        else:
            with st.spinner("Analysiere Dokumente und generiere Web-Ansicht..."):
                try:
                    # Reconstruct full text
                    all_text = "\n".join([c['text'] for c in st.session_state.text_chunks])
                    
                    # Use selected model or dynamic fallback
                    if 'model_option' in locals() and model_option != "Automatisch":
                         model_name = model_option
                    else:
                         model_name = get_generative_model_name()
                    
                    st.toast(f"Verwende Modell: {model_name}")
                    model = genai.GenerativeModel(model_name)
                    
                    # Get Prompt (Always Markdown for Web View)
                    # Pass selected language
                    prompt = get_summary_prompt("Markdown", all_text[:50000], language=summary_lang)
                    
                    # Safety Settings (Allow academic content)
                    safety = [
                        {"category": "HARM_CATEGORY_HARASSMENT", "threshold": "BLOCK_NONE"},
                        {"category": "HARM_CATEGORY_HATE_SPEECH", "threshold": "BLOCK_NONE"},
                        {"category": "HARM_CATEGORY_SEXUALLY_EXPLICIT", "threshold": "BLOCK_NONE"},
                        {"category": "HARM_CATEGORY_DANGEROUS_CONTENT", "threshold": "BLOCK_NONE"}
                    ]
                    
                    retries = 0
                    while retries < 3:
                        try:
                            response = model.generate_content(prompt, safety_settings=safety)
                            
                            st.session_state.summary_text = response.text
                            # Mark as Web Mode implicitly
                            st.session_state.last_summary_mode = "Markdown" 
                            break # Success
                        except Exception as e:
                            if "429" in str(e):
                                wait_time = 30 * (retries + 1)
                                st.warning(f"⚠️ Zu viele Anfragen (Rate Limit). Warte {wait_time} Sekunden...")
                                time.sleep(wait_time)
                                retries += 1
                                if retries == 3:
                                    st.error("API Limit erreicht. Bitte später versuchen.")
                            else:
                                raise e
                    
                except Exception as e:
                    st.error(f"Fehler bei Zusammenfassung: {e}")
    
    
    # Initialize State
    if "summary_text" not in st.session_state:
        st.session_state.summary_text = ""

    # Display Logic
    if st.session_state.summary_text:
        st.markdown("---")
        
        # 1. Render Web View (Markdown + Images)
        visual_text = st.session_state.summary_text
        if "render_visual_summary" in globals():
            visual_text = render_visual_summary(st.session_state.summary_text)
            
        st.markdown(visual_text, unsafe_allow_html=True)
        st.markdown("---")
        render_page_buttons(st.session_state.summary_text)
        
        # 2. Export Section (Bottom)
        st.subheader("📥 Export & PDF")
        col_export1, col_export2 = st.columns(2)
        
        # Option A: Official Script (LaTeX Backend)
        with col_export1:
            st.markdown("### 🎓 Offizielles Skript")
            st.caption("Professionelles Layout (wie im Bild) via LaTeX.")
            
            if st.button("Skript-PDF generieren (High-Quality)", type="primary"):
                with st.spinner("Generiere Layout & Diagramme..."):
                     try:
                         # 1. Generate LaTeX Source (Convert Web Content)
                         # Use existing Markdown summary to ensure PDF matches Web View exactly
                         source_text = st.session_state.get('summary_text', '')
                         if not source_text:
                             source_text = "\n".join([c['text'] for c in st.session_state.text_chunks])

                         model_name = get_generative_model_name()
                         model = genai.GenerativeModel(model_name)
                         
                         latex_prompt = get_summary_prompt("LaTeX", source_text[:50000])
                         
                         # Safety Settings
                         safety = [
                            {"category": "HARM_CATEGORY_HARASSMENT", "threshold": "BLOCK_NONE"},
                            {"category": "HARM_CATEGORY_HATE_SPEECH", "threshold": "BLOCK_NONE"},
                            {"category": "HARM_CATEGORY_SEXUALLY_EXPLICIT", "threshold": "BLOCK_NONE"},
                            {"category": "HARM_CATEGORY_DANGEROUS_CONTENT", "threshold": "BLOCK_NONE"}
                        ]
                         
                         resp = model.generate_content(latex_prompt, safety_settings=safety)
                         latex_code = resp.text.replace("```latex", "").replace("```", "")
                         
                         # 2. Compile
                         pdf_bytes = compile_latex_to_pdf(latex_code, fallback_markdown=source_text)
                         if pdf_bytes:
                             st.session_state.ihk_pdf = pdf_bytes
                         else:
                             st.error("Kompilierung fehlgeschlagen.")
                     except Exception as e:
                         st.error(f"Fehler: {e}")

            if "ihk_pdf" in st.session_state:
                 st.download_button("💾 PDF Herunterladen", st.session_state.ihk_pdf, "Zusammenfassung_Skript.pdf", "application/pdf")

        # Option B: Web View PDF
        with col_export2:
            st.markdown("### 📄 Web-Ansicht")
            st.caption("Schneller PDF-Druck dieser Seite.")
            if st.button("Schnell-PDF generieren"):
                with st.spinner("Drucke Web-Ansicht..."):
                    src = None
                    if "uploaded_file" in st.session_state and st.session_state.uploaded_file:
                         try: src = st.session_state.uploaded_file
                         except: pass
                         
                    pdf_data = create_markdown_pdf(visual_text, src)
                    if pdf_data:
                        st.session_state.web_pdf = pdf_data
                        
            if "web_pdf" in st.session_state:
                st.download_button("💾 Web-PDF Herunterladen", st.session_state.web_pdf, "Web_Summary.pdf", "application/pdf")
                
        # 3. Editor (Bottom, Hidden)
        st.markdown("---")
        with st.expander("✏️ Inhalt bearbeiten (Markdown)", expanded=False):
            st.caption("Änderungen hier werden sofort oben in der Vorschau angezeigt.")
            st.text_area(
                "Inhalt", 
                key="summary_text", 
                height=600,
                placeholder="Inhalt generieren..."
            )

