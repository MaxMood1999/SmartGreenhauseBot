import asyncio
import json
import logging
import threading
import time
from datetime import datetime
from typing import Dict, Any
from aiogram.types import InlineKeyboardMarkup, InlineKeyboardButton
from aiogram import Bot, Dispatcher, types
from aiogram.filters import Command
from aiogram.types import Message
import paho.mqtt.client as mqtt

from aiogram.types import ReplyKeyboardMarkup, KeyboardButton



# Konfiguratsiya
BOT_TOKEN = "7061378124:AAEoyf1iXD2J6xnTMjaudee5-o3h7SbKbM0"
ADMIN_ID = [5490986430, 101532810]
MQTT_BROKER = "test.mosquitto.org"
MQTT_PORT = 1883

# Logging sozlash
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('greenhouse_bot.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

# Bot va dispatcher
bot = Bot(token=BOT_TOKEN)
dp = Dispatcher()

# Global o'zgaruvchilar
greenhouse_state = {
    "temperature": None,
    "humidity": None,
    "soil": None,
    "water": None,
    "fan": "OFF",
    "pump": "OFF",
    "light": "OFF",
    "left_roof": "CLOSED",
    "right_roof": "CLOSED",
    "last_update": None
}

mqtt_client = None
mqtt_connected = False


def is_admin(user_id: int) -> bool:
    """Admin tekshirish"""
    return user_id in ADMIN_ID


def format_status() -> str:
    """Status formatini yaratish"""
    if not greenhouse_state["last_update"]:
        return "Malumot mavjud emas. ESP32 bilan aloqa yo'q."

    temp = greenhouse_state["temperature"]
    humidity = greenhouse_state["humidity"]
    soil = greenhouse_state["soil"]
    water = greenhouse_state["water"]

    fan_status = "Yoqilgan" if greenhouse_state["fan"] == "ON" else "O'chirilgan"
    pump_status = "Yoqilgan" if greenhouse_state["pump"] == "ON" else "O'chirilgan"
    light_status = "Yoqilgan" if greenhouse_state["light"] == "ON" else "O'chirilgan"
    left_roof_status = "Ochiq" if greenhouse_state["left_roof"] == "OPEN" else "Yopiq"
    right_roof_status = "Ochiq" if greenhouse_state["right_roof"] == "OPEN" else "Yopiq"

    temp_str = f"{temp:.1f}°C" if temp is not None else "N/A"
    humidity_str = f"{humidity:.1f}%" if humidity is not None else "N/A"
    soil_str = f"{soil}%" if soil is not None else "N/A"
    water_str = f"{water}%" if water is not None else "N/A"

    last_update = greenhouse_state["last_update"].strftime("%H:%M:%S")

    return f"""Issiqxona Holati

Sensorlar:
Harorat: {temp_str}
Namlik: {humidity_str}
Tuproq namligi: {soil_str}
Suv miqdori: {water_str}

Aktuatorlar:
Ventilator: {fan_status}
Nasos: {pump_status}
Yorug'lik: {light_status}
Chap lyuk: {left_roof_status}
O'ng lyuk: {right_roof_status}

Oxirgi yangilanish: {last_update}
MQTT holat: {"Ulangan" if mqtt_connected else "Uzilgan"}"""

# ReplyKeyboard (pastki menyu tugmasi)
def get_reply_keyboard():
    keyboard = ReplyKeyboardMarkup(
        keyboard=[
            [KeyboardButton(text="📋 Menu")]
        ],
        resize_keyboard=True,   # tugmani kichraytiradi, ekran kengligiga mos
        one_time_keyboard=False # tugma doim ko‘rinib turadi
    )
    return keyboard


def get_main_keyboard():
    keyboard = InlineKeyboardMarkup(inline_keyboard=[
        [
            InlineKeyboardButton(text="📊 Holat", callback_data="status"),
        ],
        [
            InlineKeyboardButton(text="💨 Fan ON", callback_data="fan_on"),
            InlineKeyboardButton(text="💨 Fan OFF", callback_data="fan_off"),
        ],
        [
            InlineKeyboardButton(text="💧 Pump ON", callback_data="pump_on"),
            InlineKeyboardButton(text="💧 Pump OFF", callback_data="pump_off"),
        ],
        [
            InlineKeyboardButton(text="💡 Light ON", callback_data="light_on"),
            InlineKeyboardButton(text="💡 Light OFF", callback_data="light_off"),
        ],
        # [
        #     InlineKeyboardButton(text="🔼 Roof OPEN", callback_data="roof_open"),
        #     InlineKeyboardButton(text="🔽 Roof CLOSE", callback_data="roof_close"),
        # ],
        [
            InlineKeyboardButton(text="◀️ Left OPEN", callback_data="roof_left_open"),
            InlineKeyboardButton(text="◀️ Left CLOSE", callback_data="roof_left_close"),
        ],
        [
            InlineKeyboardButton(text="▶️ Right OPEN", callback_data="roof_right_open"),
            InlineKeyboardButton(text="▶️ Right CLOSE", callback_data="roof_right_close"),
        ],
    ])
    return keyboard


def on_connect(client, userdata, flags, rc):
    """MQTT ulanish callback"""
    global mqtt_connected
    if rc == 0:
        mqtt_connected = True
        logger.info("MQTT broker ga ulandi")
        client.subscribe("greenhouse/data")
        # Status so'rash
        client.publish("greenhouse/control", "status")
    else:
        mqtt_connected = False
        logger.error(f"MQTT ulanish xatosi: {rc}")


def on_disconnect(client, userdata, rc):
    """MQTT uzilish callback"""
    global mqtt_connected
    mqtt_connected = False
    logger.warning("MQTT uzildi")


def on_message(client, userdata, msg):
    """MQTT xabar callback"""
    try:
        topic = msg.topic
        payload = msg.payload.decode()
        logger.info(f"MQTT xabar keldi: {topic} -> {payload}")

        if topic == "greenhouse/data":
            try:
                data = json.loads(payload)

                # Ma'lumotlarni yangilash
                if "temperature" in data:
                    greenhouse_state["temperature"] = float(data["temperature"])
                if "humidity" in data:
                    greenhouse_state["humidity"] = float(data["humidity"])
                if "soil" in data:
                    greenhouse_state["soil"] = int(data["soil"])
                if "water" in data:
                    greenhouse_state["water"] = int(data["water"])

                # Aktuatorlar holati
                if "fan" in data:
                    greenhouse_state["fan"] = data["fan"]
                if "pump" in data:
                    greenhouse_state["pump"] = data["pump"]
                if "light" in data:
                    greenhouse_state["light"] = data["light"]
                if "left_roof" in data:
                    greenhouse_state["left_roof"] = data["left_roof"]
                if "right_roof" in data:
                    greenhouse_state["right_roof"] = data["right_roof"]

                greenhouse_state["last_update"] = datetime.now()

            except json.JSONDecodeError as e:
                logger.error(f"JSON parse xatosi: {e}")

    except Exception as e:
        logger.error(f"MQTT xabar ishlov berishda xato: {e}")


def publish_command(command: str) -> bool:
    """MQTT buyruq yuborish"""
    try:
        if mqtt_client and mqtt_connected:
            mqtt_client.publish("greenhouse/control", command)
            logger.info(f"MQTT buyruq yuborildi: {command}")
            return True
    except Exception as e:
        logger.error(f"MQTT buyruq yuborishda xato: {e}")
    return False


def mqtt_thread():
    """MQTT uchun alohida thread"""
    global mqtt_client

    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_disconnect = on_disconnect
    mqtt_client.on_message = on_message

    while True:
        try:
            logger.info("MQTT broker ga ulanmoqda...")
            mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
            mqtt_client.loop_forever()
        except Exception as e:
            logger.error(f"MQTT ulanish xatosi: {e}")
            time.sleep(5)


def periodic_status_thread():
    """Muntazam status so'rash uchun thread"""
    while True:
        time.sleep(30)  # Har 30 soniyada
        if mqtt_connected:
            publish_command("status")


@dp.message(Command("start"))
async def cmd_start(message: Message):
    if not is_admin(message.from_user.id):
        await message.answer("Kirish taqiqlangan")
        return

    await message.answer(
        "Smart Greenhouse Bot ga xush kelibsiz!\n📋 Menu tugmasidan foydalaning.",
        reply_markup=get_reply_keyboard()   # Pastki tugmani ko‘rsatamiz
    )



# "📋 Menu" bosilganda Inline tugmalarni chiqaramiz
@dp.message(lambda msg: msg.text == "📋 Menu")
async def show_menu(message: Message):
    if not is_admin(message.from_user.id):
        await message.answer("Kirish taqiqlangan")
        return

    await message.answer(
        "Menyu tugmalari:",
        reply_markup=get_main_keyboard()   # Inline menyu
    )
@dp.callback_query()
async def process_callback(callback: types.CallbackQuery):
    action = callback.data

    if not is_admin(callback.from_user.id):
        await callback.answer("Kirish taqiqlangan", show_alert=True)
        return

    if action == "status":
        await callback.message.answer(format_status())
        publish_command("status")

    elif action in ["fan_on", "fan_off", "pump_on", "pump_off",
                    "light_on", "light_off",
                    "roof_left_open", "roof_left_close",
                    "roof_right_open", "roof_right_close"]:

        if publish_command(action):
            await callback.message.answer(f"✅ Buyruq yuborildi: {action}")
        else:
            await callback.message.answer("❌ Buyruq yuborilmadi - MQTT aloqa yo'q")

    # elif action == "roof_open":
    #     success1 = publish_command("lyuk_left_open")
    #     success2 = publish_command("lyuk_right_open")
    #     if success1 and success2:
    #         await callback.message.answer("✅ Barcha lyuklar ochildi")
    #     else:
    #         await callback.message.answer("❌ Buyruq yuborilmadi - MQTT aloqa yo'q")
    #
    # elif action == "roof_close":
    #     success1 = publish_command("lyuk_left_close")
    #     success2 = publish_command("lyuk_right_close")
    #     if success1 and success2:
    #         await callback.message.answer("✅ Barcha lyuklar yopildi")
    #     else:
    #         await callback.message.answer("❌ Buyruq yuborilmadi - MQTT aloqa yo'q")

    await callback.answer()  # Loading belgisini yo‘qotish uchun



@dp.message(Command("status"))
async def cmd_status(message: Message):
    if not is_admin(message.from_user.id):
        return

    status_text = format_status()
    await message.answer(status_text)

    # Fresh status so'rash
    publish_command("status")


# Fan komandlari
@dp.message(Command("fan_on"))
async def cmd_fan_on(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("fan_on"):
        await message.answer("Ventilator yoqildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("fan_off"))
async def cmd_fan_off(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("fan_off"):
        await message.answer("Ventilator o'chirildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


# Pump komandlari
@dp.message(Command("pump_on"))
async def cmd_pump_on(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("pump_on"):
        await message.answer("Nasos yoqildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("pump_off"))
async def cmd_pump_off(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("pump_off"):
        await message.answer("Nasos o'chirildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


# Light komandlari
@dp.message(Command("light_on"))
async def cmd_light_on(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("light_on"):
        await message.answer("Yorug'lik yoqildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("light_off"))
async def cmd_light_off(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("light_off"):
        await message.answer("Yorug'lik o'chirildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


# Roof komandlari
@dp.message(Command("roof_open"))
async def cmd_roof_open(message: Message):
    if not is_admin(message.from_user.id):
        return

    success1 = publish_command("lyuk_left_open")
    success2 = publish_command("lyuk_right_open")
    if success1 and success2:
        await message.answer("Barcha lyuklar ochildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("roof_close"))
async def cmd_roof_close(message: Message):
    if not is_admin(message.from_user.id):
        return

    success1 = publish_command("lyuk_left_close")
    success2 = publish_command("lyuk_right_close")
    if success1 and success2:
        await message.answer("Barcha lyuklar yopildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("roof_left_open"))
async def cmd_roof_left_open(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("lyuk_left_open"):
        await message.answer("Chap lyuk ochildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("roof_left_close"))
async def cmd_roof_left_close(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("lyuk_left_close"):
        await message.answer("Chap lyuk yopildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("roof_right_open"))
async def cmd_roof_right_open(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("lyuk_right_open"):
        await message.answer("O'ng lyuk ochildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message(Command("roof_right_close"))
async def cmd_roof_right_close(message: Message):
    if not is_admin(message.from_user.id):
        return

    if publish_command("lyuk_right_close"):
        await message.answer("O'ng lyuk yopildi")
    else:
        await message.answer("Buyruq yuborilmadi - MQTT aloqa yo'q")


@dp.message()
async def handle_other_messages(message: Message):
    if not is_admin(message.from_user.id):
        await message.answer("Kirish taqiqlangan")
        return

    await message.answer(
        "Noma'lum buyruq. Mavjud komandalar:\n"
        "/status - Hozirgi holat\n"
        "/fan_on, /fan_off - Ventilator\n"
        "/pump_on, /pump_off - Nasos\n"
        "/light_on, /light_off - Yorug'lik\n"
        "/roof_open, /roof_close - Barcha lyuklar\n"
        "/roof_left_open, /roof_left_close - Chap lyuk\n"
        "/roof_right_open, /roof_right_close - O'ng lyuk"
    )


async def main():
    """Asosiy funksiya"""
    logger.info("Smart Greenhouse Bot ishga tushmoqda...")

    # MQTT thread ni ishga tushirish
    mqtt_thread_obj = threading.Thread(target=mqtt_thread, daemon=True)
    mqtt_thread_obj.start()

    # Periodic status thread
    status_thread_obj = threading.Thread(target=periodic_status_thread, daemon=True)
    status_thread_obj.start()

    # Bot ishga tushirish
    try:
        await dp.start_polling(bot)
    except KeyboardInterrupt:
        logger.info("Bot to'xtatildi")
    except Exception as e:
        logger.error(f"Bot xatosi: {e}")
    finally:
        await bot.session.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Dastur to'xtatildi")