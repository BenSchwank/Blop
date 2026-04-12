#!/usr/bin/env python3
"""
Export the BTTR (Bidirectional Training with Transformer) model to ONNX for
Blop offline handwritten-math recognition.

Usage:
    # 1. Clone BTTR repo (if not done):
    #      git clone https://github.com/Green-Wood/BTTR.git
    #
    # 2. Install dependencies:
    #      pip install torch torchvision onnx onnxruntime pytorch-lightning einops
    #
    # 3. Download checkpoint (from BTTR GitHub releases):
    #      curl -L -o BTTR/pretrained-2014.ckpt \\
    #        https://github.com/Green-Wood/BTTR/releases/download/v2.0/pretrained-2014.ckpt
    #
    # 4. Run this script:
    #      python export_math_ocr_model.py \\
    #        --bttr-repo ../BTTR \\
    #        --checkpoint ../BTTR/pretrained-2014.ckpt \\
    #        --output-dir ../assets/models

Output files:
  encoder.onnx   - DenseNet encoder + positional encoding
  decoder.onnx   - Transformer decoder (L2R greedy)
  tokens.json    - Token vocabulary + model metadata
"""

import argparse
import json
import sys
from pathlib import Path

import torch
import torch.nn as nn


# ============================================================================
# ONNX Export Wrappers
# ============================================================================

class EncoderForExport(nn.Module):
    """Wraps the BTTR encoder for ONNX export.

    The original encoder expects (img, img_mask) where img_mask is a boolean
    mask indicating padded regions. For ONNX we generate the mask from the
    image automatically (padded regions = white = 1.0 after normalization
    maps to a known value, but we just use all-False = no padding).
    """
    def __init__(self, encoder):
        super().__init__()
        self.encoder = encoder

    def forward(self, pixel_values):
        # pixel_values: [B, 1, H, W]
        B, _, H, W = pixel_values.shape
        # Create mask: False = valid pixel (no padding for now;
        # C++ side will pass full images with white padding)
        img_mask = torch.zeros(B, H, W, dtype=torch.bool,
                               device=pixel_values.device)
        feature, mask = self.encoder(pixel_values, img_mask)
        # feature: [B, H'*W', d_model], mask: [B, H'*W']
        return feature


class DecoderForExport(nn.Module):
    """Wraps the BTTR decoder for ONNX export.

    Strips out the mask/padding logic and just runs forward with the
    causal attention mask that C++ will provide.
    """
    def __init__(self, decoder):
        super().__init__()
        self.decoder = decoder

    def forward(self, encoder_hidden_states, input_ids, tgt_mask):
        # encoder_hidden_states: [B, S, d_model]
        # input_ids: [B, T]
        # tgt_mask: [T, T] causal mask (bool: True = masked)

        # Embed tokens
        tgt = self.decoder.word_embed(input_ids)  # [B, T, d]
        tgt = self.decoder.pos_enc(tgt)            # [B, T, d]

        # Rearrange for PyTorch TransformerDecoder: [T, B, d] and [S, B, d]
        src = encoder_hidden_states.permute(1, 0, 2)  # [S, B, d]
        tgt = tgt.permute(1, 0, 2)                     # [T, B, d]

        # Run transformer decoder (no padding masks for simplicity)
        out = self.decoder.model(
            tgt=tgt,
            memory=src,
            tgt_mask=tgt_mask,
        )

        out = out.permute(1, 0, 2)  # [B, T, d]
        logits = self.decoder.proj(out)  # [B, T, vocab_size]
        return logits


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Export BTTR model to ONNX for Blop offline handwritten-math OCR")
    parser.add_argument("--output-dir", type=str, default="../assets/models",
                        help="Output directory for ONNX files")
    parser.add_argument("--bttr-repo", type=str, default=None,
                        help="Path to cloned BTTR repository")
    parser.add_argument("--checkpoint", type=str, required=True,
                        help="Path to pre-trained BTTR checkpoint (.ckpt)")
    parser.add_argument("--quantize", action="store_true", default=True,
                        help="Apply INT8 dynamic quantization (default: True)")
    parser.add_argument("--no-quantize", action="store_false", dest="quantize",
                        help="Skip quantization")
    parser.add_argument("--img-height", type=int, default=128,
                        help="Max input image height")
    parser.add_argument("--img-width", type=int, default=512,
                        help="Max input image width")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("Blop Math OCR Model Exporter (BTTR -> ONNX)")
    print("  Handwritten math recognition (CROHME-trained)")
    print("=" * 60)

    # ── Step 0: Add BTTR repo to path ────────────────────────────────
    bttr_repo = args.bttr_repo
    if bttr_repo is None:
        # Try common locations
        for candidate in [
            Path(__file__).parent.parent / "BTTR",
            Path.home() / "BTTR",
            Path("./BTTR"),
        ]:
            if (candidate / "bttr").is_dir():
                bttr_repo = str(candidate)
                break

    if bttr_repo:
        sys.path.insert(0, str(Path(bttr_repo).resolve()))
        print(f"  BTTR repo: {bttr_repo}")
    else:
        print("  ERROR: BTTR repository not found.")
        print("  Clone it: git clone https://github.com/Green-Wood/BTTR.git")
        print("  Then pass --bttr-repo /path/to/BTTR")
        sys.exit(1)

    # Import BTTR modules
    try:
        from bttr.datamodule import vocab, vocab_size
        from bttr.model.bttr import BTTR
        print(f"  BTTR modules loaded. Vocab size: {vocab_size}")
    except ImportError as e:
        print(f"  ERROR: Could not import BTTR: {e}")
        print("  Install dependencies: pip install pytorch-lightning einops torchvision")
        sys.exit(1)

    # ── Step 1: Load checkpoint ──────────────────────────────────────
    print(f"\n[1/4] Loading checkpoint: {args.checkpoint}")
    ckpt = torch.load(args.checkpoint, map_location="cpu", weights_only=False)

    hparams = ckpt.get("hyper_parameters", {})
    print(f"  Hyperparameters: {hparams}")

    model = BTTR(
        d_model=hparams.get("d_model", 256),
        growth_rate=hparams.get("growth_rate", 24),
        num_layers=hparams.get("num_layers", 16),
        nhead=hparams.get("nhead", 8),
        num_decoder_layers=hparams.get("num_decoder_layers", 3),
        dim_feedforward=hparams.get("dim_feedforward", 1024),
        dropout=0.0,  # no dropout at inference
    )

    # Load state dict (strip 'bttr.' prefix from Lightning checkpoint)
    state = ckpt["state_dict"]
    cleaned = {}
    for k, v in state.items():
        new_key = k.replace("bttr.", "", 1) if k.startswith("bttr.") else k
        cleaned[new_key] = v

    model.load_state_dict(cleaned, strict=False)
    model.eval()
    print(f"  Model loaded successfully.")

    # ── Step 2: Export encoder ───────────────────────────────────────
    print(f"\n[2/4] Exporting encoder (max {args.img_height}x{args.img_width})...")
    encoder_path = output_dir / "encoder.onnx"

    encoder_wrapper = EncoderForExport(model.encoder)
    encoder_wrapper.eval()

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
                dynamo=False,
            )
        enc_size = encoder_path.stat().st_size / (1024 * 1024)
        print(f"  Encoder exported: {encoder_path} ({enc_size:.1f} MB)")
    except Exception as e:
        print(f"  Encoder export failed: {e}")
        import traceback; traceback.print_exc()
        sys.exit(1)

    # ── Step 3: Export decoder ───────────────────────────────────────
    print(f"\n[3/4] Exporting decoder...")
    decoder_path = output_dir / "decoder.onnx"

    decoder_wrapper = DecoderForExport(model.decoder)
    decoder_wrapper.eval()

    # Create dummy inputs
    with torch.no_grad():
        enc_out = encoder_wrapper(dummy_img)  # [1, seq_len, d_model]

    dummy_ids = torch.tensor([[vocab.SOS_IDX]], dtype=torch.long)  # [1, 1]
    # Causal mask: [1, 1] for single token (no masking needed)
    dummy_mask = torch.zeros(1, 1, dtype=torch.bool)

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
                dynamo=False,
            )
        dec_size = decoder_path.stat().st_size / (1024 * 1024)
        print(f"  Decoder exported: {decoder_path} ({dec_size:.1f} MB)")
    except Exception as e:
        print(f"  Decoder export failed: {e}")
        import traceback; traceback.print_exc()
        sys.exit(1)

    # ── Step 3b: Quantize ────────────────────────────────────────────
    if args.quantize:
        print("\n  Applying INT8 dynamic quantization...")
        try:
            from onnxruntime.quantization import quantize_dynamic, QuantType

            enc_fp32 = output_dir / "encoder_fp32.onnx"
            dec_fp32 = output_dir / "decoder_fp32.onnx"

            encoder_path.rename(enc_fp32)
            decoder_path.rename(dec_fp32)

            quantize_dynamic(
                str(enc_fp32), str(encoder_path), weight_type=QuantType.QInt8)
            quantize_dynamic(
                str(dec_fp32), str(decoder_path), weight_type=QuantType.QInt8)

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
            enc_fp32 = output_dir / "encoder_fp32.onnx"
            dec_fp32 = output_dir / "decoder_fp32.onnx"
            if enc_fp32.exists() and not encoder_path.exists():
                enc_fp32.rename(encoder_path)
            if dec_fp32.exists() and not decoder_path.exists():
                dec_fp32.rename(decoder_path)

    # ── Step 4: Export vocabulary ────────────────────────────────────
    print(f"\n[4/4] Exporting token vocabulary...")
    tokens_path = output_dir / "tokens.json"

    # Build token list from BTTR vocab (idx -> word)
    token_list = []
    for i in range(vocab_size):
        token_list.append(vocab.idx2word.get(i, f"<unk_{i}>"))

    tokens_data = {
        "tokens": token_list,
        "bos_id": vocab.SOS_IDX,  # 1 = <sos>
        "eos_id": vocab.EOS_IDX,  # 2 = <eos>
        "pad_id": vocab.PAD_IDX,  # 0 = <pad>
        "img_channels": 1,
        "img_height": args.img_height,
        "img_width": args.img_width,
        "max_seq_len": hparams.get("max_len", 200),
        "model_name": "bttr",
        "normalization": {
            "mean": 0.5,
            "std": 0.5
        },
        "needs_tgt_mask": True
    }

    with open(tokens_path, "w", encoding="utf-8") as f:
        json.dump(tokens_data, f, ensure_ascii=False, indent=2)

    print(f"  Vocabulary exported: {vocab_size} tokens")
    print(f"    BOS={vocab.SOS_IDX} (<sos>)")
    print(f"    EOS={vocab.EOS_IDX} (<eos>)")
    print(f"    PAD={vocab.PAD_IDX} (<pad>)")
    print(f"    Tokens: {token_list[3:13]}...")

    # ── Summary ──────────────────────────────────────────────────────
    total_size = sum(
        f.stat().st_size for f in output_dir.iterdir() if f.is_file()
    ) / (1024 * 1024)

    print("\n" + "=" * 60)
    print("Export complete!")
    print(f"  Output directory: {output_dir.resolve()}")
    print(f"  Total size: {total_size:.1f} MB")
    for f in sorted(output_dir.iterdir()):
        if f.is_file():
            size = f.stat().st_size / (1024 * 1024)
            print(f"    {f.name:30s}  {size:6.1f} MB")
    print("=" * 60)
    print("\nNext steps:")
    print("  1. Copy 'assets/models/' to your Blop build directory")
    print("  2. Rebuild Blop with CMake (BLOP_HAS_ONNX_OCR will be set)")
    print("  3. Place onnxruntime.dll next to Blop.exe")
    print("  4. Test handwriting recognition in the graph formula bar")


if __name__ == "__main__":
    main()
