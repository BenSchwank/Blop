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

export const MathInline = Node.create({
    name: "mathInline",
    group: "inline",
    inline: true,
    atom: true,
    selectable: true,
    addAttributes() {
        return {
            latex: { default: "" },
        };
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
        return {
            latex: { default: "" },
        };
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
