extern "C" void main() {
  char *video = reinterpret_cast<char *>(0xB8000);
  video[0] = '!';
}
