#include "ff2/save.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScroller>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

constexpr auto weapon = ff2::ProficiencyKind::weapon;
constexpr auto magic = ff2::ProficiencyKind::magic;
constexpr auto fileFilter = "セーブファイル (*.sav *.srm);;すべてのファイル (*)";

#ifdef Q_OS_ANDROID
// Keep selectors inside the main window. Qt's Android popup can abort while
// acquiring the OpenGL surface on some devices.
class Picker final : public QTabBar {
public:
    Picker()
    {
        setExpanding(true);
        setUsesScrollButtons(false);
        setElideMode(Qt::ElideRight);
    }

    void addItem(const QString& text) { addTab(text); }
    void removeItem(int index) { removeTab(index); }
    void setItemText(int index, const QString& text) { setTabText(index, text); }
};
constexpr auto pickerChanged = &Picker::currentChanged;
#else
using Picker = QComboBox;
constexpr auto pickerChanged = &Picker::currentIndexChanged;
#endif

void updatePicker(Picker* picker, const QStringList& labels, int selected)
{
    const QSignalBlocker blocked(picker);
    for (int i = 0; i < labels.size(); ++i) {
        if (i < picker->count()) {
            picker->setItemText(i, labels[i]);
        } else {
            picker->addItem(labels[i]);
        }
    }
    while (picker->count() > labels.size()) {
        picker->removeItem(picker->count() - 1);
    }
    picker->setCurrentIndex(selected);
}

QString filePath(const QUrl& url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QByteArray readSave(const QUrl& url)
{
    QFile file(filePath(url));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(file.errorString().toStdString());
    }
    QByteArray data;
    constexpr auto limit = static_cast<qint64>(ff2::Save::sramSize) + 2;
    while (data.size() < limit) {
        const auto block = file.read(limit - data.size());
        if (file.error() != QFileDevice::NoError) {
            throw std::runtime_error(file.errorString().toStdString());
        }
        if (block.isEmpty()) {
            break;
        }
        data += block;
    }
    if (data.size() != limit - 2 && data.size() != limit - 1) {
        throw std::runtime_error("8,192または8,193バイトの非圧縮セーブを指定してください。");
    }
    return data;
}

QString writeFile(const QUrl& url, const QByteArray& data, bool replace, bool* opened = nullptr)
{
    QFile file(filePath(url));
    const QIODevice::OpenMode flags = QIODevice::WriteOnly | (url.isLocalFile() && !replace
        ? QIODevice::NewOnly : QIODevice::Truncate);
    if (!file.open(flags)) {
        return file.errorString();
    }
    if (opened) {
        *opened = true;
    }
    if (file.write(data) != data.size() || !file.flush()) {
        const auto error = file.errorString();
        file.close();
        if (url.isLocalFile() && !replace) {
            file.remove();
        }
        return error;
    }
    file.close();
    return file.error() == QFileDevice::NoError ? QString{} : file.errorString();
}

QString backupDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backups";
}

QString createBackup(const QUrl& source, const QByteArray& original)
{
    const auto folder = source.isLocalFile() ? QFileInfo(source.toLocalFile()).absolutePath()
                                             : backupDirectory();
    if (!source.isLocalFile() && !QDir().mkpath(folder)) {
        throw std::runtime_error("バックアップ用フォルダーを作成できません。");
    }
    auto name = source.fileName();
    if (name.isEmpty()) {
        name = "FF2";
    }
    QTemporaryFile backup(QDir(folder).filePath(name + ".XXXXXX.bak"));
    if (!backup.open() || backup.write(original) != original.size() || !backup.flush()) {
        throw std::runtime_error(backup.errorString().toStdString());
    }
    const auto path = backup.fileName();
    backup.close();
    if (backup.error() != QFileDevice::NoError) {
        throw std::runtime_error(backup.errorString().toStdString());
    }
    backup.setAutoRemove(false);
    return path;
}

struct ProficiencyInputs {
    QLabel* label;
    QSpinBox* level;
    QSpinBox* progress;
};

QString spellName(std::uint8_t id)
{
    // Spell IDs 0xC0–0xE7 from the legacy FF2 item and spell table.
    constexpr std::array<const char*, 40> names{
        "ファイア", "サンダー", "ブリザド", "クラウダ",
        "ドレイン", "アスピル", "フレアー", "スリプル",
        "スタン", "ストップ", "コンフュ", "ブライン",
        "カーズ", "トード", "ブレイク", "デス",
        "デジョン", "バーサク", "ヘイスト", "オーラ",
        "ケアル", "レイズ", "バスナ", "エスナ",
        "バリア", "ブリンク", "プロテス", "シェル",
        "ウォール", "デスペル", "ミニマム", "サイレス",
        "アンチ", "フォーグ", "スロウ", "チェンジ",
        "フィアー", "ホーリー", "テレポ", "アルテマ",
    };
    constexpr int firstSpellId = 0xC0;
    if (id == 0) {
        return "未習得";
    }
    if (id >= firstSpellId && id < firstSpellId + names.size()) {
        return QString::fromUtf8(names[id - firstSpellId]);
    }
    return QString("不明 (%1)").arg(id, 2, 16, QChar('0'));
}

QSpinBox* numberInput(int min, int max)
{
    auto* input = new QSpinBox;
    input->setRange(min, max);
    input->setKeyboardTracking(false);
#ifdef Q_OS_ANDROID
    input->setMinimumHeight(38);
#endif
    return input;
}

QString displayName(const std::string& name)
{
    auto text = QString::fromStdString(name);
    while (text.endsWith(QChar(0x3000))) {
        text.chop(1);
    }
    return text;
}

// Show out-of-range values from the original file without silently clamping them.
void showNumber(QSpinBox* input, int value, int max)
{
    input->setMaximum(std::max(value, max));
    input->setValue(value);
}

class Editor final : public QWidget {
public:
    Editor()
    {
        setWindowTitle("FF2 セーブエディタ");
        auto* layout = new QVBoxLayout(this);
        auto* actions = new QHBoxLayout;
        auto* open = new QPushButton("開く…");
        saveButton_ = new QPushButton("上書き保存");
        saveAsButton_ = new QPushButton("別名で保存…");
        actions->addWidget(open);
        actions->addWidget(saveButton_);
        actions->addWidget(saveAsButton_);
        layout->addLayout(actions);
#ifdef Q_OS_ANDROID
        auto* exportBackup = new QPushButton("バックアップを書き出す…");
        layout->addWidget(exportBackup);
        connect(exportBackup, &QPushButton::clicked, this, [this] { exportBackupFile(); });
#endif

        auto* selection = new QFormLayout;
        slotPicker_ = new Picker;
        characterPicker_ = new Picker;
        selection->addRow("セーブ枠", slotPicker_);
        selection->addRow("キャラクター", characterPicker_);
        layout->addLayout(selection);
        tabs_ = new QTabWidget;
        layout->addWidget(tabs_, 1);

        auto* characterForm = addPage("基本");
        gil_ = numberInput(0, ff2::Save::maxGil);
        name_ = new QLineEdit;
        name_->setMaxLength(6);
        characterForm->addRow("ギル", gil_);
        characterForm->addRow("名前", name_);
        const char* vitalsLabels[] = {"現在HP", "最大HP", "現在MP", "最大MP"};
        for (std::size_t i = 0; i < vitals_.size(); ++i) {
            vitals_[i] = numberInput(0, 65535);
            characterForm->addRow(vitalsLabels[i], vitals_[i]);
            watch(vitals_[i]);
        }

        auto* attributesForm = addPage("能力値");
        const char* attributesLabels[] = {"力", "すばやさ", "体力", "知性", "精神", "魔力"};
        for (std::size_t i = 0; i < attributes_.size(); ++i) {
            attributes_[i] = numberInput(0, 99);
            attributesForm->addRow(attributesLabels[i], attributes_[i]);
            watch(attributes_[i]);
        }

        auto* weaponsForm = addPage("武器");
        const char* weapons[] = {"素手", "盾", "短剣", "杖", "槍", "剣", "斧", "弓"};
        for (std::size_t i = 0; i < weapons_.size(); ++i) {
            weapons_[i] = addProficiency(weaponsForm, weapons[i]);
        }
        auto* magicForm = addPage("魔法");
        for (std::size_t i = 0; i < magic_.size(); ++i) {
            magic_[i] = addProficiency(magicForm, QString("魔法枠 %1").arg(i + 1), true);
        }

        status_ = new QLabel("非圧縮のセーブファイルを開いてください。");
        status_->setWordWrap(true);
        status_->setTextFormat(Qt::PlainText);
        layout->addWidget(status_);
        watch(gil_);
        connect(name_, &QLineEdit::textEdited, this, [this] { markDraft(); });
        connect(open, &QPushButton::clicked, this, [this] { openFile(); });
        connect(saveButton_, &QPushButton::clicked, this, [this] { overwriteFile(); });
        connect(saveAsButton_, &QPushButton::clicked, this, [this] { saveFileAs(); });
        connect(slotPicker_, pickerChanged, this, [this](int slot) {
            switchSelection(slot, 0);
        });
        connect(characterPicker_, pickerChanged, this, [this](int index) {
            switchSelection(slot_, index);
        });
        setEditable(false);
        tabs_->hide();
        slotPicker_->setEnabled(false);
    }

protected:
    void closeEvent(QCloseEvent* event) override
    {
        event->setAccepted(canDiscard());
    }

private:
    QFormLayout* addPage(const QString& label)
    {
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
#ifdef Q_OS_ANDROID
        QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
#endif
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        scroll->setWidget(page);
        tabs_->addTab(scroll, label);
        return form;
    }

    ProficiencyInputs addProficiency(QFormLayout* form, const QString& label,
                                     bool captionAbove = false)
    {
        auto* row = new QHBoxLayout;
        auto* level = numberInput(1, 16);
        auto* progress = numberInput(0, 99);
        auto* caption = new QLabel(label);
        level->setAccessibleName(label + " レベル");
        progress->setAccessibleName(label + " 進捗");
        row->addWidget(new QLabel("Lv"));
        row->addWidget(level);
        row->addWidget(new QLabel("進捗"));
        row->addWidget(progress);
        if (captionAbove) {
            auto* fields = new QVBoxLayout;
            caption->setWordWrap(true);
            fields->addWidget(caption);
            fields->addLayout(row);
            form->addRow(fields);
        } else {
            form->addRow(caption, row);
        }
        watch(level);
        watch(progress);
        return {caption, level, progress};
    }

    void watch(QSpinBox* input)
    {
        connect(input, &QSpinBox::valueChanged, this, [this] { markDraft(); });
    }

    void markDraft()
    {
        if (!loading_) {
            draftEdited_ = true;
            updateStatus();
        }
    }

    void setEditable(bool enabled)
    {
        characterPicker_->setEnabled(enabled);
        tabs_->setEnabled(enabled);
        saveButton_->setEnabled(save_.has_value());
        saveAsButton_->setEnabled(save_.has_value());
    }

    bool canDiscard()
    {
        if (!draftEdited_ && !dirty_) {
            return true;
        }
        return QMessageBox::question(this, "未保存の変更", "変更を破棄しますか？",
                   QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel)
            == QMessageBox::Discard;
    }

    void showError(const QString& title, const QString& detail)
    {
        QMessageBox::warning(this, title, detail);
    }

    void updateStatus()
    {
        status_->setText(fileName_ + (draftEdited_ || dirty_ ? " — 未保存" : ""));
    }

    void populate()
    {
        loading_ = true;
        QStringList slotsLabels;
        for (std::size_t slot = 1; slot <= ff2::Save::slotCount; ++slot) {
#ifdef Q_OS_ANDROID
            QString label = QString::number(slot);
            switch (save_->status(slot)) {
            case ff2::SlotStatus::empty: label += " 空"; break;
            case ff2::SlotStatus::invalid_checksum: label += " 異常"; break;
            case ff2::SlotStatus::valid: break;
            }
#else
            QString label = QString("セーブ %1").arg(slot);
            switch (save_->status(slot)) {
            case ff2::SlotStatus::empty: label += "（空）"; break;
            case ff2::SlotStatus::invalid_checksum: label += "（チェックサム不一致）"; break;
            case ff2::SlotStatus::valid: break;
            }
#endif
            slotsLabels.append(label);
        }
        updatePicker(slotPicker_, slotsLabels, slot_);
        const auto slot = slot_ + 1;
        const bool editable = save_->status(slot) == ff2::SlotStatus::valid;
        setEditable(editable);
        tabs_->setVisible(editable);
        QStringList characterLabels;
        if (editable) {
            for (std::size_t index = 0; index < ff2::Save::characterCount; ++index) {
                const auto value = save_->character(slot, index);
                const auto name = displayName(value.name);
                characterLabels.append(QString("%1. %2").arg(index + 1).arg(name));
            }
            const auto value = save_->character(slot, character_);
            name_->setText(displayName(value.name));
            gil_->setValue(save_->gil(slot));
            const std::array vitals{value.hp, value.maxHp, value.mp, value.maxMp};
            for (std::size_t i = 0; i < vitals.size(); ++i) {
                vitals_[i]->setValue(vitals[i]);
            }
            const auto attributes = save_->attributes(slot, character_);
            for (std::size_t i = 0; i < attributes.size(); ++i) {
                showNumber(attributes_[i], attributes[i], 99);
            }
            showProficiencies(weapon, weapons_);
            showProficiencies(magic, magic_);
            for (std::size_t i = 0; i < magic_.size(); ++i) {
                const auto label = QString("%1. %2").arg(i + 1).arg(
                    spellName(save_->magicId(slot, character_, i)));
                magic_[i].label->setText(label);
                magic_[i].level->setAccessibleName(label + " レベル");
                magic_[i].progress->setAccessibleName(label + " 進捗");
            }
        }
        updatePicker(characterPicker_, characterLabels, editable ? character_ : -1);
        loading_ = false;
        draftEdited_ = false;
        updateStatus();
    }

    template <std::size_t N>
    void showProficiencies(ff2::ProficiencyKind kind, const std::array<ProficiencyInputs, N>& fields)
    {
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto value = save_->proficiency(slot_ + 1, character_, kind, i);
            showNumber(fields[i].level, value.level, 16);
            showNumber(fields[i].progress, value.progress, 99);
        }
    }

    template <std::size_t N>
    void applyProficiencies(ff2::Save& edited, ff2::ProficiencyKind kind,
                            const std::array<ProficiencyInputs, N>& fields)
    {
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto old = save_->proficiency(slot_ + 1, character_, kind, i);
            const auto level = static_cast<std::uint32_t>(fields[i].level->value());
            const auto progress = static_cast<std::uint32_t>(fields[i].progress->value());
            if (level != old.level || progress != old.progress) {
                edited.setProficiency(slot_ + 1, character_, kind, i, {level, progress});
            }
        }
    }

    bool applyDraft()
    {
        if (!draftEdited_) {
            return true;
        }
        try {
            const auto slot = slot_ + 1;
            auto edited = *save_;
            const auto original = save_->character(slot, character_);
            if (name_->text() != displayName(original.name)) {
                edited.setName(slot, character_, name_->text().toStdString());
            }
            if (static_cast<std::uint32_t>(gil_->value()) != save_->gil(slot)) {
                edited.setGil(slot, gil_->value());
            }
            const std::array originalVitals{original.hp, original.maxHp, original.mp, original.maxMp};
            std::array<std::uint16_t, 4> vitals;
            for (std::size_t i = 0; i < vitals.size(); ++i) {
                vitals[i] = static_cast<std::uint16_t>(vitals_[i]->value());
            }
            if (vitals != originalVitals) {
                edited.setHpMp(slot, character_, vitals[0], vitals[1], vitals[2], vitals[3]);
            }
            ff2::Attributes attributes{};
            for (std::size_t i = 0; i < attributes.size(); ++i) {
                attributes[i] = attributes_[i]->value();
            }
            if (attributes != save_->attributes(slot, character_)) {
                edited.setAttributes(slot, character_, attributes);
            }
            applyProficiencies(edited, weapon, weapons_);
            applyProficiencies(edited, magic, magic_);
            dirty_ = dirty_ || (edited.bytes() != save_->bytes());
            save_ = std::move(edited);
            draftEdited_ = false;
            updateStatus();
            return true;
        } catch (const std::exception& error) {
            showError("入力を確認してください", QString::fromUtf8(error.what()));
            return false;
        }
    }

    void switchSelection(int slot, int character)
    {
        if (slot < 0 || character < 0) {
            return;
        }
        if (!applyDraft()) {
            const QSignalBlocker slotsBlocked(slotPicker_);
            const QSignalBlocker charactersBlocked(characterPicker_);
            slotPicker_->setCurrentIndex(slot_);
            characterPicker_->setCurrentIndex(character_);
            return;
        }
        slot_ = slot;
        character_ = character;
        populate();
    }

    void openFile()
    {
        if (!canDiscard()) {
            return;
        }
        const auto source = QFileDialog::getOpenFileUrl(this, "セーブを開く", {}, fileFilter);
        if (source.isEmpty()) {
            return;
        }
        try {
            const auto data = readSave(source);
            save_ = ff2::Save(std::vector<std::uint8_t>(data.begin(), data.end()));
        } catch (const std::exception& error) {
            showError("セーブを開けません", QString::fromUtf8(error.what()));
            return;
        }
        source_ = source;
        fileName_ = source.fileName();
        slot_ = 0;
        for (std::size_t slot = 1; slot <= ff2::Save::slotCount; ++slot) {
            if (save_->status(slot) == ff2::SlotStatus::valid) {
                slot_ = static_cast<int>(slot - 1);
                break;
            }
        }
        character_ = 0;
        dirty_ = false;
        slotPicker_->setEnabled(true);
        populate();
    }

    QByteArray editedBytes() const
    {
        const auto& bytes = save_->bytes();
        return QByteArray(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    void finishSave(const QString& message)
    {
        dirty_ = false;
        populate();
        status_->setText(message);
    }

    void overwriteFile()
    {
        if (!applyDraft() || !dirty_) {
            return;
        }
        QString backup;
        try {
            const auto original = readSave(source_);
            backup = createBackup(source_, original);
            const auto modified = editedBytes();
            if (source_.isLocalFile()) {
                QSaveFile file(source_.toLocalFile());
                if (!file.open(QIODevice::WriteOnly) || file.write(modified) != modified.size()
                    || !file.commit()) {
                    throw std::runtime_error(file.errorString().toStdString());
                }
            } else {
                bool opened = false;
                auto error = writeFile(source_, modified, true, &opened);
                if (error.isEmpty()) {
                    try {
                        if (readSave(source_) != modified) {
                            error = "書き込んだ内容を確認できません。";
                        }
                    } catch (const std::exception& readError) {
                        error = QString::fromUtf8(readError.what());
                    }
                }
                if (!error.isEmpty()) {
                    if (opened) {
                        const auto restoreError = writeFile(source_, original, true);
                        error += restoreError.isEmpty() ? QString("\n元の内容を書き戻しました。")
                            : QString("\n元の内容を書き戻せませんでした。") + restoreError;
                    }
                    throw std::runtime_error(error.toStdString());
                }
            }
        } catch (const std::exception& error) {
            auto detail = QString::fromUtf8(error.what());
            if (!backup.isEmpty()) {
                detail += "\nバックアップ: " + QFileInfo(backup).fileName();
            }
            showError("上書き保存に失敗しました", detail);
            return;
        }
        finishSave("上書き保存しました — バックアップ: " + QFileInfo(backup).fileName());
    }

    void saveFileAs()
    {
        if (!applyDraft()) {
            return;
        }
        const auto destination = QFileDialog::getSaveFileUrl(
            this, "別名で保存", QUrl::fromLocalFile(QDir::home().filePath("FF2-edited.sav")), fileFilter,
            nullptr, QFileDialog::DontConfirmOverwrite);
        if (destination.isEmpty()) {
            return;
        }
        if (destination == source_) {
            showError("別のファイル名を指定してください", "読み込んだファイルが選択されています。");
            return;
        }
        const auto error = writeFile(destination, editedBytes(), false);
        if (!error.isEmpty()) {
            showError("保存に失敗しました", error);
            return;
        }
        source_ = destination;
        fileName_ = destination.fileName();
        finishSave("保存しました — " + fileName_);
    }

#ifdef Q_OS_ANDROID
    void exportBackupFile()
    {
        const QDir folder(backupDirectory());
        const auto names = folder.entryList({"*.bak"}, QDir::Files, QDir::Time);
        if (names.isEmpty()) {
            QMessageBox::information(this, "バックアップ", "保存済みのバックアップはありません。");
            return;
        }
        bool selected = false;
        const auto name = QInputDialog::getItem(this, "バックアップ", "書き出すファイル",
                                               names, 0, false, &selected);
        if (!selected) {
            return;
        }
        const auto target = QFileDialog::getSaveFileUrl(
            this, "バックアップを書き出す", QUrl::fromLocalFile(QDir::home().filePath(name)));
        if (target.isEmpty()) {
            return;
        }
        try {
            const auto data = readSave(QUrl::fromLocalFile(folder.filePath(name)));
            const auto error = writeFile(target, data, false);
            if (!error.isEmpty()) {
                showError("バックアップを書き出せません", error);
            }
        } catch (const std::exception& error) {
            showError("バックアップを読み込めません", QString::fromUtf8(error.what()));
        }
    }
#endif

    std::optional<ff2::Save> save_;
    QUrl source_;
    QString fileName_;
    int slot_ = 0;
    int character_ = 0;
    bool loading_ = false;
    bool draftEdited_ = false;
    bool dirty_ = false;
    Picker* slotPicker_;
    Picker* characterPicker_;
    QPushButton* saveButton_;
    QPushButton* saveAsButton_;
    QTabWidget* tabs_;
    QLabel* status_;
    QLineEdit* name_;
    QSpinBox* gil_;
    std::array<QSpinBox*, 4> vitals_;
    std::array<QSpinBox*, 6> attributes_;
    std::array<ProficiencyInputs, ff2::Save::weaponCount> weapons_;
    std::array<ProficiencyInputs, ff2::Save::magicSlotCount> magic_;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    Editor editor;
#ifndef Q_OS_ANDROID
    editor.resize(640, 540);
#endif
    editor.show();
    return app.exec();
}
