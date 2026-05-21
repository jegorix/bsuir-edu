# 03 Class Diagram

Файл диаграммы:

- `03_class_diagram.pdf`
- `03_class_diagram.png`
- `03_class_diagram.svg`

Название плаката: **Диаграмма классов программы**.

## Назначение

Проект написан на языке C, поэтому диаграмма классов используется как UML-подобная схема основных структур данных и модулей, выполняющих роль классов в проектной модели.

## Условные обозначения

| Тип блока | Фигура | Где используется |
|---|---|---|
| Класс/структура | Прямоугольник из трёх секций | Имя, поля, функции |
| Связь использования | Стрелка | Один модуль использует данные другого |
| Подпись связи | Текст над стрелкой | `uses`, `edit inode`, `search`, `view` |

## Классы и структуры

| Класс/структура | Поля | Методы/операции |
|---|---|---|
| `Ext4Context` | `fd`, `image_path`, `image_size`, `write_mode`, `readonly_forced` | `open_image()`, `read_at()`, `write_at()`, `backup()` |
| `Ext4SuperView` | `offset`, `block_size`, `inode_size`, `blocks_count`, `features` | `read_primary()`, `find_backup()`, `check_features()` |
| `Ext4GroupDescView` | `group_index`, `inode_table`, `free_blocks`, `free_inodes` | `read_group_desc()` |
| `Ext4InodeView` | `inode_no`, `mode`, `uid`, `gid`, `size`, `flags`, `timestamps` | `read_inode()`, `write_inode_fields()` |
| `DirectoryEntry` | `inode`, `rec_len`, `name_len`, `file_type`, `name` | `list_dir()`, `lookup_path()`, `find_by_name()` |
| `MetadataEditRequest` | `target`, `field`, `new_value`, `inode_no` | `validate()`, `apply_super()`, `apply_inode()`, `verify()` |
| `Dashboard` | `language`, `selected_item`, `readonly/write` | `show_menu()`, `show_dialog()`, `show_result()` |
| `Util` | `le16/le32/le64`, `parse_uint` | `read_le()`, `format_time()`, `parse_input()` |

## Стрелки

1. `Ext4Context -> Ext4SuperView`: контекст образа используется для чтения superblock.
2. `Ext4SuperView -> Ext4GroupDescView`: параметры superblock определяют расположение групп.
3. `Ext4SuperView -> Ext4InodeView`: параметры inode используются для вычисления смещений.
4. `Ext4InodeView -> DirectoryEntry`: inode каталога даёт доступ к блокам directory entry.
5. `MetadataEditRequest -> Ext4InodeView`: редактирование полей inode.
6. `MetadataEditRequest -> Ext4SuperView`: редактирование полей superblock.
7. `Dashboard -> Ext4InodeView`: просмотр inode.
8. `Dashboard -> DirectoryEntry`: листинг каталогов и поиск.
9. `Dashboard -> MetadataEditRequest`: запуск редактирования.
10. `Util -> MetadataEditRequest`: разбор числового ввода.
11. `Ext4Context -> Dashboard`: общий контекст работы с образом отображается в интерфейсе.

## ASCII-эскиз

```text
+----------------+      +-----------------+      +-------------------+
| Ext4Context    | ---> | Ext4SuperView   | ---> | Ext4GroupDescView |
+----------------+      +-----------------+      +-------------------+
         |                       |
         |                       v
         |               +---------------+
         |               | Ext4InodeView |
         |               +---------------+
         |                       |
         |                       v
         |               +----------------+
         |               | DirectoryEntry |
         |               +----------------+
         |
         v
+----------------+      +---------------------+      +------+
| Dashboard      | ---> | MetadataEditRequest | ---> | Util |
+----------------+      +---------------------+      +------+
```

