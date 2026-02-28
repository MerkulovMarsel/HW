#include <stdint.h>
#include <stdio.h>
#include <string.h>

int ret(const int result, FILE *file) {
    if (file)
        fclose(file);
    if (!result)
        printf("PE\n");
    else
        printf("Not PE\n");
    return result;
}

int seek_read(FILE *file, int offset, void *address, int size, int count) {
    return !(!fseek(file, offset, SEEK_SET) &&
             fread(address, size, count, file) == count);
}

void read_print(FILE *file, uint32_t offset) {
    static char buffer[256];

    if (fseek(file, offset, SEEK_SET) != 0) {
        return;
    }

    for (int i = 0; i < 255; ++i) {
        if (fread(&buffer[i], 1, 1, file) != 1) {
            break;
        }

        if (buffer[i] == '\0') {
            break;
        }
    }

    printf("%s", buffer);
}

int is_pe(const char *path) {
    FILE *file = fopen(path, "rb");

    if (!file)
        return ret(1, file);

    uint32_t pe_address;
    if (seek_read(file, 0x3C, &pe_address, 4, 1))
        return ret(1, file);

    char is_pe_char[4];
    if (seek_read(file, pe_address, is_pe_char, 1, 4) ||
        memcmp(is_pe_char, "PE\0\0", 4))
        return ret(1, file);

    return ret(0, file);
}

int check_end(char *buffer, int size) {
    char zero_buf[20] = {0};
    return !memcmp(buffer, zero_buf, size);
}

void import_functions(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return;

    uint32_t current_address;
    if (seek_read(file, 0x3C, &current_address, 4, 1)) {
        fclose(file);
        return;
    }

    current_address += 24;

    uint32_t import_table_rva;
    if (seek_read(file, current_address + 0x78, &import_table_rva, 4, 1)) {
        fclose(file);
        return;
    }

    current_address += 240;

    uint32_t section_virtual_size, section_rva, section_raw;
    char section_buffer[16];
    do {

        if (seek_read(file, current_address + 0x8, section_buffer, 1, 16)) {
            fclose(file);
            return;
        }

        memcpy(&section_virtual_size, &section_buffer[0], 4);
        memcpy(&section_rva, &section_buffer[4], 4);
        memcpy(&section_raw, &section_buffer[12], 4);

        current_address += 40;
    } while (!(section_rva <= import_table_rva && section_rva + section_virtual_size >= import_table_rva));

    uint32_t import_offset = section_raw + import_table_rva - section_rva;

    uint32_t name_rva;
    uint32_t import_lookup_rva;
    char header_checker[20];
    while (1) {
        if (seek_read(file, import_offset, header_checker, 1, 20)) {
            fclose(file);
            return;
        }

        if (check_end(header_checker, 20)) {
            break;
        }

        memcpy(&name_rva, &header_checker[12], 4);
        uint32_t name_offset = section_raw + name_rva - section_rva;

        if (seek_read(file, import_offset, &import_lookup_rva, 4, 1)) {
            fclose(file);
            return;
        }

        read_print(file, name_offset);
        printf("\n");
        import_offset += 20;

        uint32_t import_lookup_table_offset = section_raw + import_lookup_rva - section_rva;
        uint64_t lookup_checker;
        while (1) {
            if (seek_read(file, import_lookup_table_offset, &lookup_checker, 8, 1)) {
                fclose(file);
                return;
            }

            if (check_end((char *) &lookup_checker, 8)) {
                break;
            }

            if (lookup_checker >> 63 == 0) {
                uint32_t function_rva = (uint32_t) ((lookup_checker << 33) >> 33);
                uint32_t function_offset = section_raw + function_rva - section_rva;

                printf("    ");
                read_print(file, function_offset + 2);
                printf("\n");
            }
            import_lookup_table_offset += 8;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 3)
        return -1;
    char *command = argv[1];
    char *path = argv[2];
    if (!strcmp(command, "is-pe"))
        return is_pe(path);
    if (!strcmp(command, "import-functions"))
        import_functions(path);
    return 0;
}
