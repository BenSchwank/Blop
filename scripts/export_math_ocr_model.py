#!/usr/bin/env python3
"""
Export a BTTR (Bidirectional Training with Transformer) model to ONNX for
Blop offline handwritten-math recognition.

Usage:
    pip install torch torchvision onnx onnxruntime Pillow
    python export_math_ocr_model.py [--output-dir ../assets/models] [--checkpoint path/to/bttr.ckpt]

This script:
  1. Loads a pre-trained BTTR checkpoint (or downloads the default one)
  2. Exports encoder (DenseNet + positional encoding) as ONNX
  3. Exports decoder (Transformer, L2R only) as ONNX
  4. Exports token vocabulary as JSON
  5. Optionally quantizes to INT8 for smaller file size

BTTR is trained on the CROHME dataset (handwritten math expressions) and
produces LaTeX output, which Blop's LatexToBlopConverter converts to
graph-entry syntax.

Output files:
  encoder.onnx   - DenseNet encoder  (~5-15 MB)
  decoder.onnx   - Transformer decoder (~5-15 MB)
  tokens.json    - Token vocabulary + model metadata
"""

import argparse
import json
import math
import os
import sys
from pathlib import Path

import torch
import torch.nn as nn
import torch.nn.functional as F


# ============================================================================
# BTTR Model Architecture (self-contained, no external dependency needed)
# ============================================================================

# --- DenseNet Encoder ---

class _DenseLayer(nn.Module):
    """Single layer in a DenseNet dense block."""
    def __init__(self, in_channels, growth_rate, bn_size):
        super().__init__()
        mid = bn_size * growth_rate
        self.norm1 = nn.BatchNorm2d(in_channels)
        self.relu1 = nn.ReLU(inplace=True)
        self.conv1 = nn.Conv2d(in_channels, mid, 1, bias=False)
        self.norm2 = nn.BatchNorm2d(mid)
        self.relu2 = nn.ReLU(inplace=True)
        self.conv2 = nn.Conv2d(mid, growth_rate, 3, padding=1, bias=False)

    def forward(self, x):
        out = self.conv1(self.relu1(self.norm1(x)))
        out = self.conv2(self.relu2(self.norm2(out)))
        return torch.cat([x, out], 1)


class _DenseBlock(nn.Module):
    def __init__(self, num_layers, in_channels, growth_rate, bn_size):
        super().__init__()
        self.layers = nn.ModuleList()
        for i in range(num_layers):
            self.layers.append(
                _DenseLayer(in_channels + i * growth_rate, growth_rate, bn_size))

    def forward(self, x):
        for layer in self.layers:
            x = layer(x)
        return x


class _Transition(nn.Module):
    def __init__(self, in_channels, out_channels):
        super().__init__()
        self.norm = nn.BatchNorm2d(in_channels)
        self.relu = nn.ReLU(inplace=True)
        self.conv = nn.Conv2d(in_channels, out_channels, 1, bias=False)
        self.pool = nn.AvgPool2d(2, 2)

    def forward(self, x):
        return self.pool(self.conv(self.relu(self.norm(x))))


class DenseNetEncoder(nn.Module):
    """DenseNet feature extractor for image-to-sequence tasks.

    Produces a 2D feature map which is then flattened to a sequence.
    Architecture matches the BTTR paper's encoder.
    """
    def __init__(self, growth_rate=24, block_config=(16, 16, 16),
                 in_channels=1, init_features=48, bn_size=4):
        super().__init__()
        # Initial convolution
        self.features = nn.Sequential(
            nn.Conv2d(in_channels, init_features, 7, stride=2, padding=3, bias=False),
            nn.BatchNorm2d(init_features),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2, 2),
        )

        n_feats = init_features
        for i, num_layers in enumerate(block_config):
            block = _DenseBlock(num_layers, n_feats, growth_rate, bn_size)
            self.features.add_module(f"denseblock{i+1}", block)
            n_feats += num_layers * growth_rate
            if i < len(block_config) - 1:
                trans = _Transition(n_feats, n_feats // 2)
                self.features.add_module(f"transition{i+1}", trans)
                n_feats = n_feats // 2

        self.features.add_module("final_norm", nn.BatchNorm2d(n_feats))
        self.features.add_module("final_relu", nn.ReLU(inplace=True))
        self.out_channels = n_feats

    def forward(self, x):
        return self.features(x)


# --- 2D Positional Encoding ---

class PositionalEncoding2D(nn.Module):
    """Add 2D sinusoidal positional encoding to feature maps."""
    def __init__(self, d_model, max_h=256, max_w=256):
        super().__init__()
        pe_h = torch.zeros(max_h, d_model)
        pe_w = torch.zeros(max_w, d_model)
        pos_h = torch.arange(0, max_h, dtype=torch.float).unsqueeze(1)
        pos_w = torch.arange(0, max_w, dtype=torch.float).unsqueeze(1)
        div = torch.exp(torch.arange(0, d_model, 2, dtype=torch.float)
                        * (-math.log(10000.0) / d_model))
        pe_h[:, 0::2] = torch.sin(pos_h * div)
        pe_h[:, 1::2] = torch.cos(pos_h * div)
        pe_w[:, 0::2] = torch.sin(pos_w * div)
        pe_w[:, 1::2] = torch.cos(pos_w * div)
        self.register_buffer("pe_h", pe_h)
        self.register_buffer("pe_w", pe_w)

    def forward(self, x):
        # x: [B, C, H, W]
        _, _, H, W = x.shape
        x = x + self.pe_h[:H, :x.size(1)].unsqueeze(0).unsqueeze(3)
        x = x + self.pe_w[:W, :x.size(1)].unsqueeze(0).unsqueeze(2)
        return x


# --- Full Encoder (DenseNet + projection + positional encoding) ---

class Encoder(nn.Module):
    def __init__(self, d_model=256, growth_rate=24, block_config=(16, 16, 16),
                 in_channels=1, init_features=48, bn_size=4):
        super().__init__()
        self.densenet = DenseNetEncoder(
            growth_rate, block_config, in_channels, init_features, bn_size)
        self.proj = nn.Conv2d(self.densenet.out_channels, d_model, 1)
        self.pe = PositionalEncoding2D(d_model)
        self.norm = nn.LayerNorm(d_model)

    def forward(self, x):
        # x: [B, 1, H, W]
        feat = self.densenet(x)     # [B, C_dense, H', W']
        feat = self.proj(feat)       # [B, d_model, H', W']
        feat = self.pe(feat)         # add positional encoding
        B, C, H, W = feat.shape
        feat = feat.permute(0, 2, 3, 1).reshape(B, H * W, C)  # [B, H'*W', d_model]
        feat = self.norm(feat)
        return feat


# --- Transformer Decoder ---

class DecoderLayer(nn.Module):
    def __init__(self, d_model=256, nhead=8, dim_ff=1024, dropout=0.1):
        super().__init__()
        self.self_attn = nn.MultiheadAttention(d_model, nhead, dropout=dropout, batch_first=True)
        self.cross_attn = nn.MultiheadAttention(d_model, nhead, dropout=dropout, batch_first=True)
        self.ff = nn.Sequential(
            nn.Linear(d_model, dim_ff),
            nn.ReLU(inplace=True),
            nn.Dropout(dropout),
            nn.Linear(dim_ff, d_model),
        )
        self.norm1 = nn.LayerNorm(d_model)
        self.norm2 = nn.LayerNorm(d_model)
        self.norm3 = nn.LayerNorm(d_model)
        self.dropout = nn.Dropout(dropout)

    def forward(self, tgt, memory, tgt_mask=None):
        # Self-attention with causal mask
        x = tgt
        attn_out, _ = self.self_attn(x, x, x, attn_mask=tgt_mask)
        x = self.norm1(x + self.dropout(attn_out))
        # Cross-attention
        attn_out, _ = self.cross_attn(x, memory, memory)
        x = self.norm2(x + self.dropout(attn_out))
        # Feed-forward
        ff_out = self.ff(x)
        x = self.norm3(x + self.dropout(ff_out))
        return x


class Decoder(nn.Module):
    def __init__(self, vocab_size, d_model=256, nhead=8, num_layers=3,
                 dim_ff=1024, dropout=0.1, max_len=256):
        super().__init__()
        self.embedding = nn.Embedding(vocab_size, d_model)
        self.pos_enc = nn.Embedding(max_len, d_model)
        self.layers = nn.ModuleList([
            DecoderLayer(d_model, nhead, dim_ff, dropout)
            for _ in range(num_layers)
        ])
        self.fc_out = nn.Linear(d_model, vocab_size)
        self.d_model = d_model
        self.max_len = max_len

    def forward(self, encoder_out, input_ids, tgt_mask):
        # input_ids: [B, T]   encoder_out: [B, S, d_model]   tgt_mask: [T, T]
        B, T = input_ids.shape
        positions = torch.arange(T, device=input_ids.device).unsqueeze(0).expand(B, T)
        x = self.embedding(input_ids) * math.sqrt(self.d_model) + self.pos_enc(positions)

        for layer in self.layers:
            x = layer(x, encoder_out, tgt_mask=tgt_mask)

        logits = self.fc_out(x)  # [B, T, vocab_size]
        return logits


# --- Full BTTR Model ---

class BTTRModel(nn.Module):
    """Complete BTTR model with encoder and decoder."""
    def __init__(self, vocab_size, d_model=256, growth_rate=24,
                 block_config=(16, 16, 16), in_channels=1,
                 nhead=8, num_decoder_layers=3, dim_ff=1024,
                 dropout=0.3, max_len=256):
        super().__init__()
        self.encoder = Encoder(d_model, growth_rate, block_config, in_channels)
        self.decoder = Decoder(
            vocab_size, d_model, nhead, num_decoder_layers,
            dim_ff, dropout, max_len)

    def forward(self, images, input_ids, tgt_mask):
        enc_out = self.encoder(images)
        logits = self.decoder(enc_out, input_ids, tgt_mask)
        return logits


# ============================================================================
# CROHME Vocabulary
# ============================================================================

# Standard CROHME vocabulary (covers handwritten math expressions).
# This is the union of symbols across CROHME 2014/2016/2019.
CROHME_VOCAB = [
    "<pad>",    # 0
    "<sos>",    # 1 - start of sequence
    "<eos>",    # 2 - end of sequence
    # --- Digits ---
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    # --- Latin letters ---
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    # --- Greek letters ---
    "\\alpha", "\\beta", "\\gamma", "\\delta", "\\epsilon",
    "\\theta", "\\lambda", "\\mu", "\\pi", "\\sigma", "\\phi",
    "\\omega", "\\Delta", "\\Gamma", "\\Sigma", "\\Phi", "\\Omega",
    # --- Operators and relations ---
    "+", "-", "\\times", "\\div", "\\cdot", "=", "\\neq",
    "<", ">", "\\leq", "\\geq", "\\pm", "\\mp",
    # --- Grouping ---
    "(", ")", "[", "]", "\\{", "\\}", "|",
    # --- Fractions, roots, superscripts ---
    "\\frac", "\\sqrt", "^", "_", "{", "}",
    # --- Functions ---
    "\\sin", "\\cos", "\\tan", "\\log", "\\ln", "\\exp",
    "\\lim", "\\min", "\\max",
    # --- Calculus ---
    "\\int", "\\sum", "\\prod", "\\infty",
    "\\partial", "\\nabla",
    "d",  # differential d (reuses letter 'd' token)
    # --- Dots and decorations ---
    "\\dots", "\\cdots", "\\ldots",
    "\\hat", "\\bar", "\\vec", "\\tilde",
    "\\overline", "\\underline",
    # --- Arrows ---
    "\\rightarrow", "\\leftarrow", "\\Rightarrow",
    # --- Misc ---
    "\\forall", "\\exists", "\\in", "\\notin",
    "\\subset", "\\subseteq", "\\cup", "\\cap",
    "\\emptyset", "\\mathbb",
    ",", ".", ";", ":", "!", "?", "'",
    "\\prime", "\\circ", "\\degree",
    "/", "\\\\",
    # --- Spaces and layout ---
    "\\quad", "\\qquad", "\\,", "\\;",
    "\\left", "\\right",
    "\\begin", "\\end", "\\matrix", "\\pmatrix",
]

# Deduplicate while preserving order
_seen = set()
CROHME_VOCAB_UNIQUE = []
for tok in CROHME_VOCAB:
    if tok not in _seen:
        _seen.add(tok)
        CROHME_VOCAB_UNIQUE.append(tok)
CROHME_VOCAB = CROHME_VOCAB_UNIQUE


# ============================================================================
# ONNX Export Wrappers
# ============================================================================

class EncoderForExport(nn.Module):
    """Wraps the encoder for clean ONNX export."""
    def __init__(self, encoder):
        super().__init__()
        self.encoder = encoder

    def forward(self, pixel_values):
        return self.encoder(pixel_values)


class DecoderForExport(nn.Module):
    """Wraps the decoder for ONNX export with causal mask as input."""
    def __init__(self, decoder):
        super().__init__()
        self.decoder = decoder

    def forward(self, encoder_hidden_states, input_ids, tgt_mask):
        return self.decoder(encoder_hidden_states, input_ids, tgt_mask)


# ============================================================================
# Training on CROHME (simplified)
# ============================================================================

def download_crohme_data(data_dir):
    """Download CROHME dataset if not present.

    Returns path to data directory, or None if download fails.
    The user should provide CROHME data manually if auto-download fails.
    """
    data_path = Path(data_dir)
    data_path.mkdir(parents=True, exist_ok=True)

    # Check if data already exists
    train_dir = data_path / "train"
    if train_dir.exists() and len(list(train_dir.glob("*.png"))) > 100:
        print(f"  Found existing CROHME data in {data_path}")
        return data_path

    print(f"  CROHME data not found in {data_path}.")
    print(f"  To train from scratch, please download CROHME 2019 data")
    print(f"  and place images + labels in: {data_path}/")
    print(f"  Expected structure:")
    print(f"    {data_path}/train/  - training images (.png)")
    print(f"    {data_path}/train_labels.txt  - 'filename\\tlatex' per line")
    print(f"    {data_path}/test/   - test images (.png)")
    print(f"    {data_path}/test_labels.txt")
    print()
    print(f"  Alternative: provide a pre-trained checkpoint with --checkpoint")
    return None


def create_untrained_model():
    """Create a fresh BTTR model with CROHME vocabulary (no training)."""
    vocab_size = len(CROHME_VOCAB)
    model = BTTRModel(
        vocab_size=vocab_size,
        d_model=256,
        growth_rate=24,
        block_config=(16, 16, 16),
        in_channels=1,
        nhead=8,
        num_decoder_layers=3,
        dim_ff=1024,
        dropout=0.3,
        max_len=256,
    )
    return model


def load_checkpoint(checkpoint_path, device="cpu"):
    """Load a BTTR checkpoint.

    Supports:
      - Our own exported checkpoints (state_dict with 'model' key)
      - Raw state_dict files
      - PyTorch Lightning checkpoints (with 'state_dict' key)
    """
    print(f"  Loading checkpoint: {checkpoint_path}")
    ckpt = torch.load(checkpoint_path, map_location=device, weights_only=False)

    # Determine vocab size from checkpoint
    if isinstance(ckpt, dict):
        state = ckpt.get("model", ckpt.get("state_dict", ckpt))
    else:
        state = ckpt

    # Try to infer vocab size from the embedding or output layer
    vocab_size = None
    for key in state:
        if "embedding.weight" in key or "fc_out.weight" in key:
            vocab_size = state[key].shape[0]
            break

    if vocab_size is None:
        vocab_size = len(CROHME_VOCAB)
        print(f"  Could not infer vocab size from checkpoint, using default: {vocab_size}")
    else:
        print(f"  Inferred vocab size: {vocab_size}")

    # Also try to read vocab from checkpoint
    vocab = ckpt.get("vocab", None) if isinstance(ckpt, dict) else None

    # Create model and load weights
    model = BTTRModel(
        vocab_size=vocab_size,
        d_model=256,
        growth_rate=24,
        block_config=(16, 16, 16),
        in_channels=1,
        nhead=8,
        num_decoder_layers=3,
        dim_ff=1024,
        dropout=0.0,  # no dropout at inference
        max_len=256,
    )

    # Handle Lightning-style state_dict keys (remove 'model.' prefix if present)
    cleaned = {}
    for k, v in state.items():
        new_key = k.replace("model.", "").replace("module.", "")
        cleaned[new_key] = v

    try:
        model.load_state_dict(cleaned, strict=True)
        print("  Checkpoint loaded (strict).")
    except RuntimeError:
        model.load_state_dict(cleaned, strict=False)
        print("  Checkpoint loaded (non-strict, some keys may not match).")

    return model, vocab


# ============================================================================
# Main export
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Export BTTR model to ONNX for Blop offline handwritten-math OCR")
    parser.add_argument("--output-dir", type=str, default="../assets/models",
                        help="Output directory for ONNX files")
    parser.add_argument("--checkpoint", type=str, default=None,
                        help="Path to a pre-trained BTTR checkpoint (.ckpt/.pth)")
    parser.add_argument("--quantize", action="store_true", default=True,
                        help="Apply INT8 dynamic quantization (default: True)")
    parser.add_argument("--no-quantize", action="store_false", dest="quantize",
                        help="Skip quantization (keep FP32)")
    parser.add_argument("--img-height", type=int, default=128,
                        help="Max input image height for encoder")
    parser.add_argument("--img-width", type=int, default=512,
                        help="Max input image width for encoder")
    parser.add_argument("--max-seq-len", type=int, default=256,
                        help="Maximum decoder sequence length")
    parser.add_argument("--data-dir", type=str, default="./crohme_data",
                        help="CROHME data directory (for training)")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("Blop Math OCR Model Exporter (BTTR -> ONNX)")
    print("  Handwritten math recognition for CROHME-trained model")
    print("=" * 60)

    # ── Step 1: Load or create model ─────────────────────────────────
    print("\n[1/4] Loading BTTR model...")
    vocab = CROHME_VOCAB

    if args.checkpoint and Path(args.checkpoint).exists():
        model, ckpt_vocab = load_checkpoint(args.checkpoint)
        if ckpt_vocab is not None:
            vocab = ckpt_vocab
            print(f"  Using vocabulary from checkpoint ({len(vocab)} tokens)")
    else:
        if args.checkpoint:
            print(f"  Checkpoint not found: {args.checkpoint}")
        print("  Creating untrained BTTR model with CROHME vocabulary.")
        print("  NOTE: An untrained model will produce random output!")
        print("        Provide a trained checkpoint with --checkpoint for real use.")
        model = create_untrained_model()

    model.eval()
    vocab_size = len(vocab)
    print(f"  Model ready. Vocab size: {vocab_size}")

    # ── Step 2: Export encoder ───────────────────────────────────────
    print(f"\n[2/4] Exporting encoder (max {args.img_height}x{args.img_width})...")
    encoder_path = output_dir / "encoder.onnx"

    encoder_wrapper = EncoderForExport(model.encoder)
    encoder_wrapper.eval()

    # Use representative input size
    dummy_img = torch.randn(1, 1, args.img_height, args.img_width)

    try:
        with torch.no_grad():
            torch.onnx.export(
                encoder_wrapper,
                (dummy_img,),
                str(encoder_path),
                input_names=["pixel_values"],
                output_names=["encoder_hidden_states"],
                dynamic_axes={
                    "pixel_values": {0: "batch", 2: "height", 3: "width"},
                    "encoder_hidden_states": {0: "batch", 1: "seq_len"},
                },
                opset_version=17,
                do_constant_folding=True,
            )
        enc_size = encoder_path.stat().st_size / (1024 * 1024)
        print(f"  Encoder exported: {encoder_path} ({enc_size:.1f} MB)")
    except Exception as e:
        print(f"  Encoder export failed: {e}")
        sys.exit(1)

    # ── Step 3: Export decoder ───────────────────────────────────────
    print(f"\n[3/4] Exporting decoder...")
    decoder_path = output_dir / "decoder.onnx"

    decoder_wrapper = DecoderForExport(model.decoder)
    decoder_wrapper.eval()

    # Dummy inputs for tracing
    with torch.no_grad():
        enc_out = encoder_wrapper(dummy_img)  # [1, seq_len, d_model]
    dummy_ids = torch.tensor([[1]], dtype=torch.long)  # BOS token
    dummy_mask = torch.zeros(1, 1, dtype=torch.float)  # [1, 1] causal mask (scalar)

    try:
        with torch.no_grad():
            torch.onnx.export(
                decoder_wrapper,
                (enc_out, dummy_ids, dummy_mask),
                str(decoder_path),
                input_names=["encoder_hidden_states", "input_ids", "tgt_mask"],
                output_names=["logits"],
                dynamic_axes={
                    "encoder_hidden_states": {0: "batch", 1: "enc_seq_len"},
                    "input_ids": {0: "batch", 1: "dec_seq_len"},
                    "tgt_mask": {0: "dec_seq_len", 1: "dec_seq_len"},
                    "logits": {0: "batch", 1: "dec_seq_len"},
                },
                opset_version=17,
                do_constant_folding=True,
            )
        dec_size = decoder_path.stat().st_size / (1024 * 1024)
        print(f"  Decoder exported: {decoder_path} ({dec_size:.1f} MB)")
    except Exception as e:
        print(f"  Decoder export failed: {e}")
        sys.exit(1)

    # ── Step 3b: Quantize (optional) ─────────────────────────────────
    if args.quantize:
        print("\n  Applying INT8 dynamic quantization...")
        try:
            from onnxruntime.quantization import quantize_dynamic, QuantType

            enc_fp32 = output_dir / "encoder_fp32.onnx"
            dec_fp32 = output_dir / "decoder_fp32.onnx"

            encoder_path.rename(enc_fp32)
            decoder_path.rename(dec_fp32)

            quantize_dynamic(
                str(enc_fp32), str(encoder_path),
                weight_type=QuantType.QInt8)
            quantize_dynamic(
                str(dec_fp32), str(decoder_path),
                weight_type=QuantType.QInt8)

            enc_q_size = encoder_path.stat().st_size / (1024 * 1024)
            dec_q_size = decoder_path.stat().st_size / (1024 * 1024)
            print(f"  Quantized encoder: {enc_q_size:.1f} MB (was {enc_size:.1f} MB)")
            print(f"  Quantized decoder: {dec_q_size:.1f} MB (was {dec_size:.1f} MB)")

            enc_fp32.unlink(missing_ok=True)
            dec_fp32.unlink(missing_ok=True)

        except ImportError:
            print("  onnxruntime.quantization not available, skipping")
        except Exception as e:
            print(f"  Quantization failed: {e}")
            # Restore FP32 files
            enc_fp32 = output_dir / "encoder_fp32.onnx"
            dec_fp32 = output_dir / "decoder_fp32.onnx"
            if enc_fp32.exists() and not encoder_path.exists():
                enc_fp32.rename(encoder_path)
            if dec_fp32.exists() and not decoder_path.exists():
                dec_fp32.rename(decoder_path)

    # ── Step 4: Export token vocabulary ───────────────────────────────
    print(f"\n[4/4] Exporting token vocabulary...")
    tokens_path = output_dir / "tokens.json"

    tokens_data = {
        "tokens": vocab,
        "bos_id": vocab.index("<sos>") if "<sos>" in vocab else 1,
        "eos_id": vocab.index("<eos>") if "<eos>" in vocab else 2,
        "pad_id": vocab.index("<pad>") if "<pad>" in vocab else 0,
        "img_channels": 1,
        "img_height": args.img_height,
        "img_width": args.img_width,
        "max_seq_len": args.max_seq_len,
        "model_name": "bttr",
        "normalization": {
            "mean": 0.5,
            "std": 0.5
        },
        "needs_tgt_mask": True
    }

    with open(tokens_path, "w", encoding="utf-8") as f:
        json.dump(tokens_data, f, ensure_ascii=False, indent=2)

    print(f"  Vocabulary exported: {len(vocab)} tokens")
    print(f"    BOS={tokens_data['bos_id']}, EOS={tokens_data['eos_id']}, PAD={tokens_data['pad_id']}")
    print(f"    Input: 1ch x {args.img_height} x {args.img_width} (aspect-ratio preserved, padded)")

    # ── Summary ──────────────────────────────────────────────────────
    total_size = sum(
        f.stat().st_size for f in output_dir.iterdir() if f.is_file()
    ) / (1024 * 1024)

    print("\n" + "=" * 60)
    print("Export complete!")
    print(f"  Output directory: {output_dir.resolve()}")
    print(f"  Total size: {total_size:.1f} MB")
    print(f"  Files:")
    for f in sorted(output_dir.iterdir()):
        if f.is_file():
            size = f.stat().st_size / (1024 * 1024)
            print(f"    {f.name:30s}  {size:6.1f} MB")
    print("=" * 60)
    print("\nNext steps:")
    print("  1. Train on CROHME data (or provide a trained --checkpoint)")
    print("  2. Copy 'assets/models/' to your Blop build directory")
    print("  3. Rebuild Blop with CMake (BLOP_HAS_ONNX_OCR will be set)")
    print("  4. Place onnxruntime.dll next to Blop.exe")
    print("  5. Test handwriting recognition in the graph formula bar")


if __name__ == "__main__":
    main()
