import { Node, mergeAttributes } from "@tiptap/core";
import katex from "katex";

function renderLatex(latex: string, display: boolean): string {
    try {
        return katex.renderToString(latex || "", {
            throwOnError: false,
            strict: false,
            displayMode: display,
        });
    } catch {
        return latex || "";
    }
}

const latexAttr = {
    default: "",
    parseHTML: (el: HTMLElement) => el.getAttribute("data-latex") || "",
    renderHTML: (attrs: { latex?: string }) => (attrs.latex ? { "data-latex": attrs.latex } : {}),
};

export const MathInline = Node.create({
    name: "mathInline",
    group: "inline",
    inline: true,
    atom: true,
    selectable: true,
    addAttributes() {
        return { latex: latexAttr };
    },
    parseHTML() {
        return [{ tag: 'span[data-math="inline"][data-latex]' }];
    },
    renderHTML({ node }) {
        return [
            "span",
            mergeAttributes({
                class: "blop-math-inline",
                "data-math": "inline",
                "data-latex": node.attrs.latex,
                contenteditable: "false",
            }),
        ];
    },
    addNodeView() {
        return ({ node }) => {
            const dom = document.createElement("span");
            dom.className = "blop-math-inline";
            dom.setAttribute("data-math", "inline");
            dom.setAttribute("data-latex", node.attrs.latex || "");
            dom.setAttribute("contenteditable", "false");
            dom.innerHTML = renderLatex(node.attrs.latex || "", false);
            return { dom };
        };
    },
});

export const MathDisplay = Node.create({
    name: "mathDisplay",
    group: "block",
    atom: true,
    selectable: true,
    addAttributes() {
        return { latex: latexAttr };
    },
    parseHTML() {
        return [{ tag: 'span[data-math="display"][data-latex]' }, { tag: 'div[data-math="display"][data-latex]' }];
    },
    renderHTML({ node }) {
        return [
            "div",
            mergeAttributes({
                class: "blop-math-display",
                "data-math": "display",
                "data-latex": node.attrs.latex,
                contenteditable: "false",
            }),
        ];
    },
    addNodeView() {
        return ({ node }) => {
            const dom = document.createElement("div");
            dom.className = "blop-math-display";
            dom.setAttribute("data-math", "display");
            dom.setAttribute("data-latex", node.attrs.latex || "");
            dom.setAttribute("contenteditable", "false");
            dom.innerHTML = renderLatex(node.attrs.latex || "", true);
            return { dom };
        };
    },
});

export function editorMathNodesIntact(editor: { state: { doc: { descendants: (fn: (node: { type: { name: string }; attrs: { latex?: string } }) => void) => void } } }): boolean {
    let ok = true;
    let sawMath = false;
    editor.state.doc.descendants((node) => {
        if (node.type.name !== "mathInline" && node.type.name !== "mathDisplay") return;
        sawMath = true;
        if (!String(node.attrs.latex || "").trim()) ok = false;
    });
    return !sawMath || ok;
}
