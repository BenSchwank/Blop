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
import uuid
from PyPDF2 import PdfReader

from dotenv import load_dotenv
from langchain_text_splitters import RecursiveCharacterTextSplitter
from langchain_community.vectorstores import FAISS
from langchain_google_genai import GoogleGenerativeAIEmbeddings
from fpdf import FPDF
import markdown
from data_manager import DataManager
from auth_manager import AuthManager


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

# --- Navigation State ---
if "current_page" not in st.session_state:
    st.session_state.current_page = "dashboard"
if "current_folder" not in st.session_state:
    st.session_state.current_folder = None

def navigate_to(page, folder=None):
    st.session_state.current_page = page
    st.session_state.current_folder = folder
    st.rerun()

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


# --- UI COMPONENTS ---

def render_settings_overlay():
    with st.popover("⚙️"):
        st.write("### Einstellungen")
        api_key_input = st.text_input("Gemini API Key", type="password", key="api_key_input")
        if api_key_input:
            os.environ["GOOGLE_API_KEY"] = api_key_input
            genai.configure(api_key=api_key_input)
            st.success("Gespeichert!")
        
        # Model Selector
        models = ["Automatisch", "gemini-1.5-flash", "gemini-1.5-pro"]
        sel = st.selectbox("AI-Modell", models, key="model_selector")
        st.session_state.model_option = sel
        
        if st.button("Logout", type="primary"):
            AuthManager.logout()

def _run_analysis(username, folder_id):
    """Helper to analyze all PDFs in the folder."""
    pdfs = DataManager.list_pdfs(username, folder_id)
    if not pdfs: return
    
    full_paths = [DataManager.get_pdf_path(p, username, folder_id) for p in pdfs]
    
    with st.spinner(f"{len(pdfs)} Dokumente werden analysiert..."):
        try:
            # Create file-like objects with 'name' attribute for get_pdf_text compatibility
            class FileObj:
                def __init__(self, path):
                    self.path = path
                    self.name = os.path.basename(path)
                    self._f = open(path, "rb")
                def read(self): return self._f.read()
                def seek(self, n): return self._f.seek(n)
                def close(self): self._f.close()
            
            file_objs = [FileObj(p) for p in full_paths]
            
            # Re-use existing analysis logic
            raw_text = get_pdf_text(file_objs) 
            chunks = get_text_chunks(raw_text)
            index, valid_chunks = build_vector_store(chunks)
            
            st.session_state.vector_index = index
            st.session_state.text_chunks = valid_chunks
            # Cleanup
            for f in file_objs: f.close()
            st.rerun()
            
        except Exception as e:
            st.error(f"Fehler bei Analyse: {e}")

def render_file_manager(username, folder_id):
    st.subheader("📂 Datei-Manager")
    
    # 1. List Files
    pdfs = DataManager.list_pdfs(username, folder_id)
    
    # 2. Upload Area
    uploaded = st.file_uploader("PDFs hinzufügen", type=["pdf"], accept_multiple_files=True, key=f"up_{folder_id}", label_visibility="collapsed")
    if uploaded:
        for f in uploaded:
            DataManager.save_pdf(f, username, folder_id)
        st.toast("Gespeichert!")
        st.rerun()

    st.divider()
    
    # 3. List & Delete (Mini list)
    if pdfs:
        for p in pdfs:
            c1, c2 = st.columns([4, 1])
            c1.caption(f"📄 {p}")
            # Optional: Delete button logic here later
            
        st.divider()
        
        # 4. Analysis Trigger
        has_analysis = "text_chunks" in st.session_state
        if has_analysis:
            st.success(f"✅ {len(pdfs)} aktiv")
            if st.button("🔄 Neu analysieren", use_container_width=True):
                _run_analysis(username, folder_id)
        else:
            if st.button("🚀 Analysieren", type="primary", use_container_width=True):
                _run_analysis(username, folder_id)
    else:
        st.info("Ordner ist leer. Lade PDFs hoch!")

# --- GOOGLE AUTH HELPERS ---
def get_google_auth_url():
    try:
        client_id = st.secrets["google"]["client_id"]
        redirect_uri = st.secrets["google"]["redirect_uri"]
        return f"https://accounts.google.com/o/oauth2/v2/auth?response_type=code&client_id={client_id}&redirect_uri={redirect_uri}&scope=openid%20email%20profile"
    except: return None

def get_google_user(code):
    try:
        # Exchange Code
        token_url = "https://oauth2.googleapis.com/token"
        data = {
            "code": code,
            "client_id": st.secrets["google"]["client_id"],
            "client_secret": st.secrets["google"]["client_secret"],
            "redirect_uri": st.secrets["google"]["redirect_uri"],
            "grant_type": "authorization_code"
        }
        r = requests.post(token_url, data=data)
        token = r.json().get("access_token")
        
        # Get User Info
        user_info = requests.get("https://www.googleapis.com/oauth2/v1/userinfo", headers={"Authorization": f"Bearer {token}"}).json()
        return user_info
    except Exception as e:
        st.error(f"Auth Error: {e}")
        return None

# --- DASHBOARD & WORKSPACE LOGIC ---
def render_login_screen():
    st.title("🔐 Blop Study Anmeldung")
    
    # Check for Google Callback
    if "code" in st.query_params:
        code = st.query_params["code"]
        with st.spinner("Verbinde mit Google..."):
            user_info = get_google_user(code)
            if user_info:
                email = user_info.get("email")
                name = user_info.get("name", email)
                
                # Auto-Register Google User in AuthManager if new
                # We use email as username so data is consistent
                # Generate random secure password since they use Google to login
                AuthManager.register(email, str(uuid.uuid4()))
                
                st.session_state.authenticated = True
                st.session_state.username = email
                
                st.success(f"Willkommen, {name}!")
                st.query_params.clear()
                st.rerun()

    col1, col2 = st.columns([1,1])
    
    with col1:
        st.subheader("Einloggen")
        
        # Google Button
        auth_url = get_google_auth_url()
        if auth_url:
            st.markdown(f'''
            <a href="{auth_url}" target="_top" style="text-decoration:none;">
                <button style="width:100%; border-radius:8px; border:1px solid #ccc; padding:10px; background-color:white; color:#333; cursor:pointer; font-weight:bold; display:flex; align_items:center; justify-content:center; gap:10px;">
                <img src="https://upload.wikimedia.org/wikipedia/commons/c/c1/Google_%22G%22_logo.svg" style="width:20px; height:20px;">
                Mit Google anmelden
                </button>
            </a>
            <div style="text-align:center; margin-top:15px; margin-bottom:15px; color:#666;">- oder -</div>
            ''', unsafe_allow_html=True)

        l_user = st.text_input("Benutzername", key="login_user")
        l_pass = st.text_input("Passwort", type="password", key="login_pass")
        if st.button("Login", type="primary"):
            if AuthManager.login(l_user, l_pass):
                st.session_state.authenticated = True
                st.session_state.username = l_user
                st.success(f"Willkommen {l_user}!")
                st.rerun()
            else:
                st.error("Falscher Benutzername oder Passwort.")

    with col2:
        st.subheader("Registrieren")
        r_user = st.text_input("Neuer Benutzername", key="reg_user")
        r_pass = st.text_input("Neues Passwort", type="password", key="reg_pass")
        if st.button("Konto erstellen"):
            if r_user and r_pass:
                if AuthManager.register(r_user, r_pass):
                    st.success("Konto erstellt! Bitte einloggen.")
                else:
                    st.error("Benutzername vergeben.")
            else:
                st.warning("Bitte alles ausfüllen.")

def render_dashboard():
    # Load Data
    username = st.session_state.get("username", "default")
    data = DataManager.load(username)
    
    # Top Header
    c1, c2 = st.columns([6, 1])
    c1.title(f"👋 Hallo, {username}")
    with c2:
        render_settings_overlay()
    
    st.markdown("### 📁 Meine Projekte")
    
    # Grid Layout for Folders
    cols = st.columns(4)
    
    # "New Folder" Button
    with cols[0]:
        with st.popover("➕ Neues Projekt", use_container_width=True):
            new_folder_name = st.text_input("Name")
            if st.button("Erstellen", type="primary"):
                DataManager.create_folder(new_folder_name, username)
                st.rerun()
                
    # Render folders
    for i, folder in enumerate(data.get("folders", [])):
        col_idx = (i + 1) % 4
        with cols[col_idx]:
            if st.button(f"📁 {folder['name']}", key=folder['id'], use_container_width=True):
                # Clean state
                if "text_chunks" in st.session_state: del st.session_state.text_chunks
                if "vector_index" in st.session_state: del st.session_state.vector_index
                navigate_to("workspace", folder=folder['id'])

    # Admin Panel (Only for admin_)
    if username == "admin_":
        st.divider()
        with st.expander("👮 Admin Panel", expanded=False):
            st.warning("⚠️ Nutzerverwaltung")
            users = AuthManager.get_all_users()
            st.metric("Registrierte Nutzer", len(users))
            # User List
            for u in users:
                if u != "admin_":
                    c_a, c_b = st.columns([5, 1])
                    c_a.write(f"👤 {u}")
                    if c_b.button("🗑️", key=f"del_{u}"):
                        AuthManager.delete_user(u)
                        st.rerun()

def render_workspace():
    # Top Navigation Bar
    nav_col1, nav_col2, nav_col3 = st.columns([1, 4, 1])
    
    with nav_col1:
        if st.button("⬅️ Dashboard", type="secondary", use_container_width=True):
            navigate_to("dashboard")
            
    with nav_col2:
        folder_id = st.session_state.current_folder
        folder_name = "Workspace"
        username = st.session_state.get("username", "default")
        
        if folder_id:
            data = DataManager.load(username)
            folder = next((f for f in data["folders"] if f["id"] == folder_id), None)
            if folder: folder_name = folder['name']
            
        st.markdown(f"### 📂 {folder_name}")

    with nav_col3:
        render_settings_overlay()

    st.divider()

    # Main Split Layout
    left_col, right_col = st.columns([1, 2])
    
    with left_col:
        render_file_manager(username, folder_id)

    with right_col:
        # Show tools ONLY if analyzed
        if "text_chunks" in st.session_state:
            _render_tools_tabs(username, folder_id)
        else:
            st.info("👈 Bitte lade PDFs hoch und klicke auf 'Analysieren', um zu starten.")
            st.image("https://placehold.co/600x400?text=Warte+auf+Analyse", use_container_width=True)

def _render_tools_tabs(username, folder_id):
    tab1, tab2, tab3, tab4 = st.tabs(["📅 Lernplan", "💬 Dozenten-Chat", "📝 Quiz", "📑 Zusammenfassung"])
    import datetime
    
    # --- TAB 1: Lernplan ---
    with tab1:
        st.header("Smarter Lernplan")
        col1, col2 = st.columns(2)
        with col1:
            start_date = st.date_input("Start-Datum", min_value=datetime.date.today(), value=datetime.date.today())
        with col2:
            exam_date = st.date_input("Prüfungs-Datum", min_value=datetime.date.today() + datetime.timedelta(days=1), value=datetime.date.today() + datetime.timedelta(days=7))
            
        focus_topic = st.text_input("Fokus-Thema (optional)", placeholder="z.B. Differentialgleichungen")
        
        st.subheader("Lern-Intervall")
        available_days = ["Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag"]
        selected_days = st.multiselect("Lern-Tage", available_days, default=["Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag"])
        
        # Calculate dates
        study_dates = []
        current = start_date
        while current <= exam_date:
            day_name = available_days[current.weekday()]
            if day_name in selected_days:
                study_dates.append(current)
            current += datetime.timedelta(days=1)
        
        days_count = len(study_dates)
        
        if st.button("Lernplan erstellen (AI)"):
            if days_count == 0:
                st.error("Bitte Wochentage wählen!")
            else:
                st.info(f"Erstelle Plan für {days_count} Sessions...")
                chunks = st.session_state.text_chunks
                # Sampling logic inline
                sample = chunks[:2] + random.sample(chunks[2:], min(3, len(chunks)-2)) if len(chunks) > 2 else chunks
                context_text = ""
                for c in sample: context_text += f"\n[Seite {c.get('page', '?')}] {c.get('text', '')[:1000]}\n"
                
                date_list_str = ", ".join([d.strftime('%d.%m.%Y') for d in study_dates])
                prompt = f"""Erstelle Lernplan ({days_count} Einheiten) für: {date_list_str}. Fokus: {focus_topic}. Inhalt: {context_text}.
                Output nur JSON: [{{ "date": "DD.MM.YYYY", "topic": "...", "details": "Markdown..." }}]"""
                
                try:
                    model_name = st.session_state.get("model_option", "Automatisch")
                    if model_name == "Automatisch": model_name = get_generative_model_name()
                    
                    with st.spinner("AI arbeitet..."):
                        resp = genai.GenerativeModel(model_name).generate_content(prompt)
                        content = resp.text.replace("```json", "").replace("```", "").strip()
                        if content.startswith("json"): content = content[4:]
                        st.session_state.plan_data = json.loads(content)
                        st.rerun()
                except Exception as e:
                    st.error(f"Fehler: {e}")

        # Render Plan
        if "plan_data" in st.session_state:
            st.subheader(f"Dein Fahrplan")
            if st.button("➕ Tag hinzufügen"):
                st.session_state.plan_data.append({"date": "Neu", "topic": "Neu", "details": "..."})
                st.rerun()
                
            for i, item in enumerate(st.session_state.plan_data):
                with st.expander(f"📅 {item.get('date')} - {item.get('topic')}"):
                    st.markdown(item.get('details', ''))
                    if st.button("Löschen", key=f"pland_{i}"):
                        st.session_state.plan_data.pop(i)
                        st.rerun()

    # --- TAB 2: Chat ---
    with tab2:
        st.header("Dozenten-Chat")
        if "messages" not in st.session_state: st.session_state.messages = []
        for msg in st.session_state.messages:
            st.chat_message(msg["role"]).markdown(msg["content"])
            
        if prompt := st.chat_input("Frage stellen..."):
            st.session_state.messages.append({"role": "user", "content": prompt})
            st.chat_message("user").markdown(prompt)
            with st.spinner("..."):
                ans, ctx = answer_question(prompt, st.session_state.vector_index, st.session_state.text_chunks)
                st.session_state.messages.append({"role": "assistant", "content": ans})
                st.chat_message("assistant").markdown(ans)

    # --- TAB 3: Quiz ---
    with tab3:
        st.header("Quiz")
        if st.button("Neues Quiz generieren"):
            with st.spinner("..."):
                q = generate_quiz(st.session_state.text_chunks)
                if q: 
                    st.session_state.quiz_data = q
                    st.rerun()
        
        if "quiz_data" in st.session_state:
            for i, q in enumerate(st.session_state.quiz_data):
                st.markdown(f"**{i+1}. {q['question']}**")
                c = st.radio(f"Antwort {i+1}", q['options'], key=f"q_{i}", label_visibility="collapsed")
                if st.button(f"Prüfen", key=f"qk_{i}"):
                    if c == q['answer']: st.success("Richtig!")
                    else: st.error(f"Falsch. {q['answer']}")
                st.divider()

    # --- TAB 4: Summary ---
    with tab4:
        st.header("Zusammenfassung")
        lang = st.selectbox("Sprache", ["Deutsch", "English"], key="sum_lang")
        
        if st.button("Erstellen (AI)"):
             with st.spinner("Analysiere..."):
                 all_text = "\n".join([c['text'] for c in st.session_state.text_chunks])[:50000]
                 prompt = get_summary_prompt("Markdown", all_text, language=lang)
                 try:
                     model = genai.GenerativeModel(get_generative_model_name())
                     resp = model.generate_content(prompt)
                     st.session_state.summary_text = resp.text
                 except Exception as e:
                     st.error(f"Fehler: {e}")
        
        if st.session_state.get("summary_text"):
            st.markdown(st.session_state.summary_text)
            if st.button("💾 Speichern"):
                # Determine title
                pdfs = DataManager.list_pdfs(username, folder_id)
                title = pdfs[0] if pdfs else "Summary"
                DataManager.save_summary(title, st.session_state.summary_text, username, folder_id)
                st.toast("Gespeichert")

# Execute Layout

def main():
    if not AuthManager.check_login():
        render_login_screen()
        return

    if st.session_state.current_page == "dashboard":
        render_dashboard()
    elif st.session_state.current_page == "workspace":
        render_workspace()

if __name__ == "__main__":
    main()
