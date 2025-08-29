import asyncio
import logging
from aiogram import Bot, Dispatcher, F
from aiogram.types import Message, InlineKeyboardMarkup, InlineKeyboardButton
from aiogram.filters import Command
from aiogram.utils.formatting import Text, Bold, Italic, Code, Pre
from datetime import datetime

# Bot tokenini kiriting
BOT_TOKEN = "7608613333:AAFWKH0ct8AFdfQTIngXe5MUXHHJkICQHZk"

# Logging sozlash
logging.basicConfig(level=logging.INFO)

# Bot va Dispatcher yaratish
bot = Bot(token=BOT_TOKEN)
dp = Dispatcher()


# Start komandasi
@dp.message(Command("start"))
async def start_handler(message: Message):
    """Bot boshlanganda ko'rsatiladigan xabar"""

    welcome_text = Text(
        "🤖 ", Bold("Telegram ID Bot"), "ga xush kelibsiz!\n\n",
        "Bu bot orqali siz quyidagi ma'lumotlarni olishingiz mumkin:\n\n",
        "🆔 ", Bold("Telegram ID"), " - sizning noyob raqamingiz\n",
        "👤 ", Bold("Foydalanuvchi ma'lumotlari"), " - ism va username\n",
        "📅 ", Bold("Chat ma'lumotlari"), " - chat turi va vaqt\n\n",
        Italic("ID ni olish uchun istalgan xabar yuboring yoki tugmani bosing!")
    )

    # Tugmalar
    keyboard = InlineKeyboardMarkup(
        inline_keyboard=[
            [
                InlineKeyboardButton(
                    text="🆔 Mening ID im",
                    callback_data="get_my_id"
                )
            ],
            [
                InlineKeyboardButton(
                    text="ℹ️ Yordam",
                    callback_data="help"
                ),
                InlineKeyboardButton(
                    text="📊 Bot haqida",
                    callback_data="about"
                )
            ]
        ]
    )

    await message.answer(
        **welcome_text.as_kwargs(),
        reply_markup=keyboard
    )


# ID ni ko'rsatish funksiyasi
async def show_user_id(message: Message):
    """Foydalanuvchi ID sini chiroyli formatda ko'rsatish"""

    user = message.from_user
    chat = message.chat
    current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # Chat turi aniqlash
    chat_type_emoji = {
        'private': '👤',
        'group': '👥',
        'supergroup': '🏢',
        'channel': '📢'
    }

    chat_type_name = {
        'private': 'Shaxsiy chat',
        'group': 'Oddiy guruh',
        'supergroup': 'Superguruh',
        'channel': 'Kanal'
    }

    # Foydalanuvchi ma'lumotlari
    user_info = Text(
        "🎯 ", Bold("SIZNING MA'LUMOTLARINGIZ"), "\n",
        "━━━━━━━━━━━━━━━━━━━━━━━\n\n",

        "🆔 ", Bold("Telegram ID: "), Code(str(user.id)), "\n",
        "👤 ", Bold("Ism: "), user.first_name or "❌",
        f" {user.last_name}" if user.last_name else "", "\n"
    )

    if user.username:
        user_info += Text("📱 ", Bold("Username: "), Code(f"@{user.username}"), "\n")
    else:
        user_info += Text("📱 ", Bold("Username: "), "❌ Yo'q\n")

    user_info += Text(
        f"🤖 ", Bold("Bot mu: "), "✅ Ha" if user.is_bot else "❌ Yo'q", "\n",
        f"👑 ", Bold("Premium: "), "✅ Ha" if user.is_premium else "❌ Yo'q", "\n\n",

        chat_type_emoji.get(chat.type, '💬'), " ", Bold("Chat ma'lumotlari"), "\n",
        "━━━━━━━━━━━━━━━━━━━━━━━\n",
        "🆔 ", Bold("Chat ID: "), Code(str(chat.id)), "\n",
        "📊 ", Bold("Chat turi: "), chat_type_name.get(chat.type, chat.type), "\n",
        "⏰ ", Bold("Vaqt: "), current_time, "\n\n",

        "💡 ", Italic("ID ni nusxalash uchun ustiga bosing!")
    )

    # Tugmalar
    keyboard = InlineKeyboardMarkup(
        inline_keyboard=[
            [
                InlineKeyboardButton(
                    text="📋 ID ni nusxalash",
                    callback_data=f"copy_id_{user.id}"
                )
            ],
            [
                InlineKeyboardButton(
                    text="🔄 Yangilash",
                    callback_data="refresh_id"
                ),
                InlineKeyboardButton(
                    text="🏠 Bosh sahifa",
                    callback_data="back_to_start"
                )
            ]
        ]
    )

    return user_info, keyboard


# Har qanday xabar
@dp.message(F.text)
async def any_message_handler(message: Message):
    """Har qanday matnli xabarga javob"""
    user_info, keyboard = await show_user_id(message)

    await message.answer(
        **user_info.as_kwargs(),
        reply_markup=keyboard
    )


# Callback query handlerlari
@dp.callback_query(F.data == "get_my_id")
async def get_id_callback(callback_query):
    """ID olish tugmasi bosilganda"""
    await callback_query.answer("🆔 Sizning ID ingiz!")

    # Message obyektini yaratish
    message = callback_query.message
    message.from_user = callback_query.from_user
    message.chat = callback_query.message.chat

    user_info, keyboard = await show_user_id(message)

    await callback_query.message.edit_text(
        **user_info.as_kwargs(),
        reply_markup=keyboard
    )


@dp.callback_query(F.data.startswith("copy_id_"))
async def copy_id_callback(callback_query):
    """ID ni nusxalash"""
    user_id = callback_query.data.split("_")[-1]
    await callback_query.answer(f"✅ ID nusxalandi: {user_id}", show_alert=True)


@dp.callback_query(F.data == "refresh_id")
async def refresh_callback(callback_query):
    """Ma'lumotlarni yangilash"""
    await callback_query.answer("🔄 Ma'lumotlar yangilandi!")

    message = callback_query.message
    message.from_user = callback_query.from_user
    message.chat = callback_query.message.chat

    user_info, keyboard = await show_user_id(message)

    await callback_query.message.edit_text(
        **user_info.as_kwargs(),
        reply_markup=keyboard
    )


@dp.callback_query(F.data == "help")
async def help_callback(callback_query):
    """Yordam bo'limi"""
    await callback_query.answer()

    help_text = Text(
        "ℹ️ ", Bold("YORDAM"), "\n",
        "━━━━━━━━━━━━━━━━━━━━━━━\n\n",

        "📝 ", Bold("Qanday ishlaydi:"), "\n",
        "• Botga istalgan xabar yuboring\n",
        "• Bot sizning ID va ma'lumotlaringizni ko'rsatadi\n",
        "• Tugmalar orqali qo'shimcha amallar bajarishingiz mumkin\n\n",

        "🔧 ", Bold("Buyruqlar:"), "\n",
        "• ", Code("/start"), " - Botni qayta ishga tushirish\n",
        "• Istalgan matn - ID ni ko'rsatish\n\n",

        "💡 ", Bold("Maslahatlar:"), "\n",
        "• ID raqamini nusxalash uchun tugmani ishlating\n",
        "• Bot hamma turdagi chatlarda ishlaydi\n",
        "• Ma'lumotlar real vaqtda yangilanadi"
    )

    keyboard = InlineKeyboardMarkup(
        inline_keyboard=[
            [
                InlineKeyboardButton(
                    text="🏠 Bosh sahifa",
                    callback_data="back_to_start"
                )
            ]
        ]
    )

    await callback_query.message.edit_text(
        **help_text.as_kwargs(),
        reply_markup=keyboard
    )


@dp.callback_query(F.data == "about")
async def about_callback(callback_query):
    """Bot haqida ma'lumot"""
    await callback_query.answer()

    about_text = Text(
        "🤖 ", Bold("BOT HAQIDA"), "\n",
        "━━━━━━━━━━━━━━━━━━━━━━━\n\n",

        "📛 ", Bold("Nom: "), "Telegram ID Bot\n",
        "⚡ ", Bold("Versiya: "), "3.0 (Aiogram 3.x)\n",
        "👨‍💻 ", Bold("Yaratuvchi: "), "Claude AI\n",
        "🎯 ", Bold("Maqsad: "), "Telegram ID larini aniqlash\n\n",

        "🔥 ", Bold("Imkoniyatlar:"), "\n",
        "• Telegram ID ni ko'rsatish\n",
        "• Foydalanuvchi ma'lumotlari\n",
        "• Chat statistikalari\n",
        "• Chiroyli interfeys\n",
        "• Tezkor ishlash\n\n",

        "🛡️ ", Bold("Xavfsizlik: "), "Ma'lumotlar saqlanmaydi"
    )

    keyboard = InlineKeyboardMarkup(
        inline_keyboard=[
            [
                InlineKeyboardButton(
                    text="🏠 Bosh sahifa",
                    callback_data="back_to_start"
                )
            ]
        ]
    )

    await callback_query.message.edit_text(
        **about_text.as_kwargs(),
        reply_markup=keyboard
    )


@dp.callback_query(F.data == "back_to_start")
async def back_to_start_callback(callback_query):
    """Bosh sahifaga qaytish"""
    await callback_query.answer()

    welcome_text = Text(
        "🤖 ", Bold("Telegram ID Bot"), "ga xush kelibsiz!\n\n",
        "Bu bot orqali siz quyidagi ma'lumotlarni olishingiz mumkin:\n\n",
        "🆔 ", Bold("Telegram ID"), " - sizning noyob raqamingiz\n",
        "👤 ", Bold("Foydalanuvchi ma'lumotlari"), " - ism va username\n",
        "📅 ", Bold("Chat ma'lumotlari"), " - chat turi va vaqt\n\n",
        Italic("ID ni olish uchun istalgan xabar yuboring yoki tugmani bosing!")
    )

    keyboard = InlineKeyboardMarkup(
        inline_keyboard=[
            [
                InlineKeyboardButton(
                    text="🆔 Mening ID im",
                    callback_data="get_my_id"
                )
            ],
            [
                InlineKeyboardButton(
                    text="ℹ️ Yordam",
                    callback_data="help"
                ),
                InlineKeyboardButton(
                    text="📊 Bot haqida",
                    callback_data="about"
                )
            ]
        ]
    )

    await callback_query.message.edit_text(
        **welcome_text.as_kwargs(),
        reply_markup=keyboard
    )


# Sticker, rasm va boshqa turdagi xabarlar
@dp.message()
async def other_messages_handler(message: Message):
    """Boshqa turdagi xabarlarga javob"""
    user_info, keyboard = await show_user_id(message)

    # Xabar turi haqida qo'shimcha ma'lumot
    message_type = ""
    if message.sticker:
        message_type = "🎭 Sticker"
    elif message.photo:
        message_type = "🖼️ Rasm"
    elif message.video:
        message_type = "🎥 Video"
    elif message.audio:
        message_type = "🎵 Audio"
    elif message.voice:
        message_type = "🎤 Voice"
    elif message.document:
        message_type = "📎 Fayl"
    elif message.animation:
        message_type = "🎞️ GIF"
    else:
        message_type = "💬 Xabar"

    extra_info = Text(
        f"\n📤 ", Bold("Yuborilgan: "), message_type
    )

    user_info += extra_info

    await message.answer(
        **user_info.as_kwargs(),
        reply_markup=keyboard
    )


# Botni ishga tushirish
async def main():
    """Bot ishga tushirish funksiyasi"""
    print("🚀 Bot ishga tushmoqda...")
    print("📝 Bot tokenini kiriting!")
    print("⚡ Aiogram 3.x versiyasi ishlatilmoqda")

    try:
        await dp.start_polling(bot)
    except Exception as e:
        print(f"❌ Xatolik: {e}")
    finally:
        await bot.session.close()


if __name__ == "__main__":
    asyncio.run(main())