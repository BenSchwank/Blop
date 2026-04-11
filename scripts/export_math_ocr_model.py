#!/usr/bin/env python3
"""
Export the pix2tex LaTeX-OCR model to ONNX format for Blop offline recognition.

Usage:
    pip install pix2tex[gui] onnx onnxruntime
    python export_math_ocr_model.py [--output-dir ../assets/models]

This script:
  1. Loads the default pix2tex checkpoint
  2. Exports the encoder and decoder as separate ONNX files
  3. Exports the token vocabulary as JSON
  4. Optionally quantizes to INT8 for smaller file size

Output files:
  encoder.onnx       - Vision encoder  (~20-40 MB, ~8-15 MB quantized)
  decoder.onnx       - Transformer decoder (~20-40 MB, ~10-20 MB quantized)
  tokens.json        - Token vocabulary + model metadata
"""

import argparse
import json
import os
import sys
from pathlib import Path

import torch
import torch.nn as nn


def main():
    parser = argparse.ArgumentParser(
        description="Export pix2tex model to ONNX for Blop offline math OCR")
    parser.add_argument("--output-dir", type=str, default="../assets/models",
                        help="Output directory for ONNX files")
    parser.add_argument("--quantize", action="store_true", default=True,
                        help="Apply INT8 dynamic quantization (default: True)")
    parser.add_argument("--no-quantize", action="store_false", dest="quantize",
                        help="Skip quantization (keep FP32)")
    parser.add_argument("--img-height", type=int, default=224,
                        help="Fixed input image height for encoder")
    parser.add_argument("--img-width", type=int, default=224,
                        help="Fixed input image width for encoder")
    parser.add_argument("--max-seq-len", type=int, default=512,
                        help="Maximum decoder sequence length")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("Blop Math OCR Model Exporter (pix2tex → ONNX)")
    print("=" * 60)

    # ── Step 1: Load pix2tex model ────────────────────────────────────
    print("\n[1/4] Loading pix2tex model...")
    try:
        from pix2tex.cli import LatexOCR
        model_wrapper = LatexOCR()
        model = model_wrapper.model
        model.eval()
        tokenizer = model_wrapper.tokenizer
        print(f"  ✓ Model loaded. Vocab size: {len(tokenizer)}")
    except ImportError:
        print("  ✗ pix2tex not installed. Run: pip install pix2tex[gui]")
        sys.exit(1)
    except Exception as e:
        print(f"  ✗ Failed to load model: {e}")
        sys.exit(1)

    # ── Step 2: Export encoder ────────────────────────────────────────
    print(f"\n[2/4] Exporting encoder ({args.img_height}×{args.img_width})...")
    encoder_path = output_dir / "encoder.onnx"

    class EncoderWrapper(nn.Module):
        """Wraps pix2tex encoder for clean ONNX export."""
        def __init__(self, encoder):
            super().__init__()
            self.encoder = encoder

        def forward(self, pixel_values):
            return self.encoder(pixel_values)

    try:
        encoder_wrapper = EncoderWrapper(model.encoder)
        encoder_wrapper.eval()

        dummy_img = torch.randn(1, 1, args.img_height, args.img_width)

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
        print(f"  ✓ Encoder exported: {encoder_path} ({enc_size:.1f} MB)")
    except Exception as e:
        print(f"  ✗ Encoder export failed: {e}")
        print("    Trying alternative export method...")
        try:
            # Alternative: try exporting the full encoder differently
            # Some pix2tex versions have a different structure
            encoder_wrapper = EncoderWrapper(model.encoder)
            encoder_wrapper.eval()

            # Try with 3-channel input (some models expect RGB)
            dummy_img = torch.randn(1, 3, args.img_height, args.img_width)
            torch.onnx.export(
                encoder_wrapper,
                (dummy_img,),
                str(encoder_path),
                input_names=["pixel_values"],
                output_names=["encoder_hidden_states"],
                opset_version=17,
                do_constant_folding=True,
            )
            enc_size = encoder_path.stat().st_size / (1024 * 1024)
            print(f"  ✓ Encoder exported (3ch): {encoder_path} ({enc_size:.1f} MB)")
        except Exception as e2:
            print(f"  ✗ Alternative export also failed: {e2}")
            sys.exit(1)

    # ── Step 3: Export decoder ────────────────────────────────────────
    print(f"\n[3/4] Exporting decoder...")
    decoder_path = output_dir / "decoder.onnx"

    class DecoderWrapper(nn.Module):
        """Wraps pix2tex decoder for ONNX export (single forward step)."""
        def __init__(self, decoder):
            super().__init__()
            self.decoder = decoder

        def forward(self, encoder_hidden_states, input_ids):
            # The decoder uses encoder output as cross-attention context
            # and input_ids as the previous tokens
            return self.decoder(input_ids, context=encoder_hidden_states)

    try:
        decoder_wrapper = DecoderWrapper(model.decoder)
        decoder_wrapper.eval()

        # Dummy inputs
        with torch.no_grad():
            enc_out = encoder_wrapper(dummy_img)
        dummy_ids = torch.tensor([[1]], dtype=torch.long)  # BOS token

        torch.onnx.export(
            decoder_wrapper,
            (enc_out, dummy_ids),
            str(decoder_path),
            input_names=["encoder_hidden_states", "input_ids"],
            output_names=["logits"],
            dynamic_axes={
                "encoder_hidden_states": {0: "batch", 1: "enc_seq_len"},
                "input_ids": {0: "batch", 1: "dec_seq_len"},
                "logits": {0: "batch", 1: "dec_seq_len"},
            },
            opset_version=17,
            do_constant_folding=True,
        )
        dec_size = decoder_path.stat().st_size / (1024 * 1024)
        print(f"  ✓ Decoder exported: {decoder_path} ({dec_size:.1f} MB)")
    except Exception as e:
        print(f"  ✗ Decoder export failed: {e}")
        print("\n  This might be due to the pix2tex model architecture.")
        print("  Please check the pix2tex version and model structure.")
        print("  You can also try the TrOCR-based alternative (see README).")
        sys.exit(1)

    # ── Step 3b: Quantize (optional) ─────────────────────────────────
    if args.quantize:
        print("\n  Applying INT8 dynamic quantization...")
        try:
            from onnxruntime.quantization import quantize_dynamic, QuantType

            enc_q_path = output_dir / "encoder_fp32.onnx"
            dec_q_path = output_dir / "decoder_fp32.onnx"

            # Rename originals
            encoder_path.rename(enc_q_path)
            decoder_path.rename(dec_q_path)

            # Quantize
            quantize_dynamic(
                str(enc_q_path), str(encoder_path),
                weight_type=QuantType.QInt8
            )
            quantize_dynamic(
                str(dec_q_path), str(decoder_path),
                weight_type=QuantType.QInt8
            )

            # Report sizes
            enc_q_size = encoder_path.stat().st_size / (1024 * 1024)
            dec_q_size = decoder_path.stat().st_size / (1024 * 1024)
            print(f"  ✓ Quantized encoder: {enc_q_size:.1f} MB (was {enc_size:.1f} MB)")
            print(f"  ✓ Quantized decoder: {dec_q_size:.1f} MB (was {dec_size:.1f} MB)")

            # Clean up FP32 originals
            enc_q_path.unlink(missing_ok=True)
            dec_q_path.unlink(missing_ok=True)

        except ImportError:
            print("  ⚠ onnxruntime.quantization not available, skipping quantization")
            print("    Install: pip install onnxruntime")
        except Exception as e:
            print(f"  ⚠ Quantization failed: {e}")
            print("    Proceeding with FP32 models.")
            # Restore originals if rename happened
            enc_q_path = output_dir / "encoder_fp32.onnx"
            dec_q_path = output_dir / "decoder_fp32.onnx"
            if enc_q_path.exists() and not encoder_path.exists():
                enc_q_path.rename(encoder_path)
            if dec_q_path.exists() and not decoder_path.exists():
                dec_q_path.rename(decoder_path)

    # ── Step 4: Export token vocabulary ────────────────────────────────
    print(f"\n[4/4] Exporting token vocabulary...")
    tokens_path = output_dir / "tokens.json"

    try:
        # Extract token list from the tokenizer
        # pix2tex uses a custom tokenizer; we need to get the vocabulary
        vocab = []
        bos_id = -1
        eos_id = -1
        pad_id = 0

        if hasattr(tokenizer, 'itos'):
            # x-transformers style tokenizer
            vocab = list(tokenizer.itos)
        elif hasattr(tokenizer, 'get_vocab'):
            # HuggingFace-style
            v = tokenizer.get_vocab()
            vocab = [""] * len(v)
            for tok, idx in v.items():
                if idx < len(vocab):
                    vocab[idx] = tok
        elif hasattr(tokenizer, 'decoder'):
            # Another common pattern
            vocab = [tokenizer.decoder.get(i, f"<unk_{i}>")
                     for i in range(len(tokenizer))]
        else:
            # Fallback: try to iterate
            try:
                vocab = [str(tokenizer.decode([i])) for i in range(len(tokenizer))]
            except:
                print("  ⚠ Could not extract vocabulary automatically.")
                print("    Please manually create tokens.json.")
                vocab = [f"tok_{i}" for i in range(8000)]

        # Find special tokens
        for i, tok in enumerate(vocab):
            tok_lower = tok.lower().strip()
            if tok_lower in ("<s>", "[bos]", "<bos>"):
                bos_id = i
            elif tok_lower in ("</s>", "[eos]", "<eos>"):
                eos_id = i
            elif tok_lower in ("<pad>", "[pad]"):
                pad_id = i

        # Detect input channels from model
        img_channels = 1
        try:
            first_layer = list(model.encoder.parameters())[0]
            img_channels = first_layer.shape[1]
        except:
            pass

        tokens_data = {
            "tokens": vocab,
            "bos_id": bos_id if bos_id >= 0 else 1,
            "eos_id": eos_id if eos_id >= 0 else 2,
            "pad_id": pad_id,
            "img_channels": img_channels,
            "img_height": args.img_height,
            "img_width": args.img_width,
            "max_seq_len": args.max_seq_len,
            "model_name": "pix2tex",
            "normalization": {
                "mean": 0.7931,
                "std": 0.1738
            }
        }

        with open(tokens_path, "w", encoding="utf-8") as f:
            json.dump(tokens_data, f, ensure_ascii=False, indent=2)

        print(f"  ✓ Vocabulary exported: {len(vocab)} tokens")
        print(f"    BOS={bos_id}, EOS={eos_id}, PAD={pad_id}")
        print(f"    Input: {img_channels}ch × {args.img_height} × {args.img_width}")

    except Exception as e:
        print(f"  ✗ Token export failed: {e}")
        sys.exit(1)

    # ── Summary ───────────────────────────────────────────────────────
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
    print("  1. Copy the 'assets/models/' folder to your Blop build directory")
    print("  2. Rebuild Blop with CMake (BLOP_HAS_ONNX_OCR will be set)")
    print("  3. Place onnxruntime.dll next to Blop.exe")
    print("  4. Test handwriting recognition in the graph formula bar")


if __name__ == "__main__":
    main()
