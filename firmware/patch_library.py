import os

# Caminho para o arquivo psram_unique_ptr.hpp
psram_file = os.path.join('.pio', 'libdeps', 'esp32-s3-devkitc-1', 'ESP32-audioI2S', 'src', 'psram_unique_ptr.hpp')

if not os.path.exists(psram_file):
    print(f"Arquivo não encontrado: {psram_file}")
    exit(1)

# Ler o conteúdo atual
with open(psram_file, 'r', encoding='utf-8') as f:
    content = f.read()

# Verificar se já foi patcheado
if 'SPAN_POLYFILL_PATCHED' in content:
    print("Arquivo já foi patcheado anteriormente")
    exit(0)

# Conteúdo do span polyfill
span_polyfill = """// SPAN_POLYFILL_PATCHED
#ifndef SPAN_POLYFILL_H
#define SPAN_POLYFILL_H
#include <cstddef>
#include <type_traits>
#if !defined(__cpp_lib_span)
namespace std {
    template <typename T>
    class span {
    public:
        using element_type = T;
        using value_type = typename std::remove_cv<T>::type;
        using size_type = std::size_t;
        using pointer = T*;
        using reference = T&;
        using iterator = pointer;
        constexpr span() noexcept : ptr_(nullptr), size_(0) {}
        constexpr span(pointer ptr, size_type count) noexcept : ptr_(ptr), size_(count) {}
        template <std::size_t N>
        constexpr span(element_type (&arr)[N]) noexcept : ptr_(arr), size_(N) {}
        constexpr iterator begin() const noexcept { return ptr_; }
        constexpr iterator end() const noexcept { return ptr_ + size_; }
        constexpr reference operator[](size_type idx) const noexcept { return ptr_[idx]; }
        constexpr pointer data() const noexcept { return ptr_; }
        constexpr size_type size() const noexcept { return size_; }
        constexpr bool empty() const noexcept { return size_ == 0; }
        constexpr span subspan(size_type offset, size_type count = static_cast<size_type>(-1)) const noexcept {
            return span(ptr_ + offset, count == static_cast<size_type>(-1) ? size_ - offset : count);
        }
    private:
        pointer ptr_;
        size_type size_;
    };
}
#endif
#endif
"""

# Encontrar onde inserir (após #pragma once ou primeiro #include)
lines = content.split('\n')
insert_pos = 0

for i, line in enumerate(lines):
    if line.strip().startswith('#pragma once'):
        insert_pos = i + 1
        break
    elif line.strip().startswith('#ifndef') and 'PSRAM_UNIQUE_PTR' in line:
        insert_pos = i + 1
        break

# Inserir o polyfill
lines.insert(insert_pos, span_polyfill)

# Salvar o arquivo modificado
with open(psram_file, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))

print(f"✅ Arquivo {psram_file} patcheado com sucesso!")
