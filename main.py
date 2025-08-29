import asyncio
from aiogram import Bot, Dispatcher, types
from aiogram.filters import Command
from aiogram.types import Message


TOKEN = "7061378124:AAEoyf1iXD2J6xnTMjaudee5-o3h7SbKbM0"


bot = Bot(token=TOKEN)
dp = Dispatcher()


@dp.message(Command("start"))
async def start_handler(message: Message):
    await message.answer("Salom! 👋 Men aiogram yordamida yozilgan botman.")


@dp.message()
async def echo_handler(message: Message):
    await message.answer(f"Siz yozdingiz: {message.text}")

async def main():
    await dp.start_polling(bot)

if __name__ == "__main__":
    asyncio.run(main())
