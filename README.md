# YurLaps Decoder clean-room build

Upload the contents of this folder to the decoder repository root. The workflow is in `.github/workflows/build-yurlaps-decoder.yml`.

Run the workflow from GitHub Actions. It builds `YurLapsDecoder.exe` with RTL-SDR and ZeroMQ enabled. The native decoder accepts YLP-1 coherent-BPSK frames, OPN frames, and legacy AMB RC3 frames. YLP-1 uses CRC16 and tail-biting FEC. Repeated YLP-1 hits are grouped in a 40 ms window. RC4, RC4 Hybrid, Vostok, and learning messages are ignored.

The source in this folder is an independent implementation. It does not contain OpenStint source files or an OpenStint attribution file. External libraries used by the Windows build retain their own licenses.
